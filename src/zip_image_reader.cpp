#include "zip_image_reader.h"

#include "image_decode.h"
#include "runtime_log.h"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include "filesystem_compat.h"
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#ifdef HAVE_LIBZIP
#include <zip.h>

namespace {

std::string NormalizeZipPath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  std::vector<std::string> parts;
  size_t cursor = 0;
  while (cursor <= path.size()) {
    const size_t slash = path.find('/', cursor);
    std::string part = path.substr(cursor, slash == std::string::npos ? std::string::npos : slash - cursor);
    if (part.empty() || part == ".") {
      // skip
    } else if (part == "..") {
      if (!parts.empty()) parts.pop_back();
    } else {
      parts.push_back(std::move(part));
    }
    if (slash == std::string::npos) break;
    cursor = slash + 1;
  }
  std::string out;
  for (const auto &part : parts) {
    if (!out.empty()) out.push_back('/');
    out += part;
  }
  return out;
}

bool IsLikelyImagePath(const std::string &path) {
  std::string ext;
  try {
    ext = std::filesystem::path(path).extension().string();
  } catch (...) {
    return false;
  }
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".webp" ||
         ext == ".bmp" || ext == ".gif";
}

bool IsIgnoredEntryPath(const std::string &path) {
  if (path.empty()) return true;
  if (path.back() == '/') return true;
  if (path == "__MACOSX" || path.rfind("__MACOSX/", 0) == 0) return true;
  const size_t slash = path.find_last_of('/');
  const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
  return name.empty() || name.rfind("._", 0) == 0;
}

bool ReadZipEntry(zip_t *za, const std::string &name, std::vector<unsigned char> &out) {
  zip_stat_t st{};
  zip_int64_t index = zip_name_locate(za, name.c_str(), 0);
  if (index < 0) {
    index = zip_name_locate(za, name.c_str(), ZIP_FL_NOCASE);
  }
  if (index < 0) return false;
  if (zip_stat_index(za, static_cast<zip_uint64_t>(index), 0, &st) != 0) return false;
  if (st.size > static_cast<zip_uint64_t>(128 * 1024 * 1024)) return false;
  zip_file_t *zf = zip_fopen_index(za, static_cast<zip_uint64_t>(index), 0);
  if (!zf) return false;
  out.resize(static_cast<size_t>(st.size));
  size_t total = 0;
  while (total < out.size()) {
    const zip_int64_t rd = zip_fread(zf, out.data() + total, out.size() - total);
    if (rd < 0) {
      zip_fclose(zf);
      out.clear();
      return false;
    }
    if (rd == 0) break;
    total += static_cast<size_t>(rd);
  }
  zip_fclose(zf);
  out.resize(total);
  return !out.empty();
}

bool ReadZipEntryPrefix(zip_t *za, const std::string &name, size_t max_bytes, std::vector<unsigned char> &out) {
  zip_int64_t index = zip_name_locate(za, name.c_str(), 0);
  if (index < 0) {
    index = zip_name_locate(za, name.c_str(), ZIP_FL_NOCASE);
  }
  if (index < 0) return false;
  zip_file_t *zf = zip_fopen_index(za, static_cast<zip_uint64_t>(index), 0);
  if (!zf) return false;
  out.resize(max_bytes);
  size_t total = 0;
  while (total < out.size()) {
    const zip_int64_t rd = zip_fread(zf, out.data() + total, out.size() - total);
    if (rd < 0) {
      zip_fclose(zf);
      out.clear();
      return false;
    }
    if (rd == 0) break;
    total += static_cast<size_t>(rd);
  }
  zip_fclose(zf);
  out.resize(total);
  return !out.empty();
}

bool ImagePerfLogEnabled() {
  const char *env = std::getenv("ROCREADER_IMAGE_PERF_LOG");
  return env && *env && std::string(env) != "0";
}

struct PageEntry {
  std::string image_entry;
  int width = 0;
  int height = 0;
  bool size_known = false;
};

} // namespace

struct ZipImageReader::Impl {
  zip_t *zip = nullptr;
  std::string path;
  std::vector<PageEntry> pages;
  int current_page = 0;
};
#endif

bool ZipImageReader::Open(const std::string &path) {
  Close();
#ifdef HAVE_LIBZIP
  int zerr = 0;
  zip_t *za = zip_open(path.c_str(), ZIP_RDONLY, &zerr);
  if (!za) {
    runtime_log::Line("[zip_image] zip_open failed path=" + path + " zerr=" + std::to_string(zerr));
    return false;
  }

  std::vector<PageEntry> pages;
  const zip_int64_t count = zip_get_num_entries(za, 0);
  for (zip_int64_t i = 0; i < count; ++i) {
    const char *raw_name = zip_get_name(za, static_cast<zip_uint64_t>(i), 0);
    if (!raw_name) continue;
    const std::string entry_name = NormalizeZipPath(raw_name);
    if (IsIgnoredEntryPath(entry_name) || !IsLikelyImagePath(entry_name)) continue;
    pages.push_back(PageEntry{entry_name});
  }

  if (pages.empty()) {
    runtime_log::Line("[zip_image] no image pages path=" + path);
    zip_close(za);
    return false;
  }

  runtime_log::Line("[zip_image] image pages path=" + path +
                    " pages=" + std::to_string(pages.size()) +
                    " first=" + pages.front().image_entry);

  impl_ = new Impl();
  impl_->zip = za;
  impl_->path = path;
  impl_->pages = std::move(pages);
  impl_->current_page = 0;
  return true;
#else
  (void)path;
  return false;
#endif
}

void ZipImageReader::Close() {
#ifdef HAVE_LIBZIP
  if (!impl_) return;
  if (impl_->zip) {
    zip_close(impl_->zip);
    impl_->zip = nullptr;
  }
  delete impl_;
  impl_ = nullptr;
#else
  path_.clear();
  page_count_ = 0;
  current_page_ = 0;
#endif
}

bool ZipImageReader::IsOpen() const {
#ifdef HAVE_LIBZIP
  return impl_ && impl_->zip && !impl_->pages.empty();
#else
  return false;
#endif
}

bool ZipImageReader::HasRealRenderer() const {
#ifdef HAVE_LIBZIP
  return true;
#else
  return false;
#endif
}

const char *ZipImageReader::BackendName() const {
#ifdef HAVE_LIBZIP
  return "zip-image";
#else
  return "none";
#endif
}

int ZipImageReader::PageCount() const {
#ifdef HAVE_LIBZIP
  return impl_ ? static_cast<int>(impl_->pages.size()) : 0;
#else
  return 0;
#endif
}

int ZipImageReader::CurrentPage() const {
#ifdef HAVE_LIBZIP
  return impl_ ? impl_->current_page : 0;
#else
  return current_page_;
#endif
}

void ZipImageReader::SetPage(int page_index) {
  const int count = PageCount();
  if (count <= 0) return;
  page_index = std::clamp(page_index, 0, count - 1);
#ifdef HAVE_LIBZIP
  impl_->current_page = page_index;
#else
  current_page_ = page_index;
#endif
}

void ZipImageReader::NextPage() { SetPage(CurrentPage() + 1); }
void ZipImageReader::PrevPage() { SetPage(CurrentPage() - 1); }

bool ZipImageReader::PageSize(int page_index, int &w, int &h) const {
  if (!IsOpen()) return false;
  page_index = std::clamp(page_index, 0, PageCount() - 1);
#ifdef HAVE_LIBZIP
  const PageEntry &entry = impl_->pages[page_index];
  if (entry.size_known && entry.width > 0 && entry.height > 0) {
    w = entry.width;
    h = entry.height;
    return true;
  }

  std::vector<unsigned char> bytes;
  if (!ReadZipEntryPrefix(impl_->zip, entry.image_entry, 64 * 1024, bytes)) {
    runtime_log::Line("[zip_image] page size read failed page=" + std::to_string(page_index) +
                      " entry=" + entry.image_entry);
    return false;
  }
  int probed_w = 0;
  int probed_h = 0;
  if (!ProbeImageSizeFromMemory(bytes.data(), bytes.size(), probed_w, probed_h)) {
    runtime_log::Line("[zip_image] page size probe failed page=" + std::to_string(page_index) +
                      " entry=" + entry.image_entry +
                      " bytes=" + std::to_string(bytes.size()));
    return false;
  }
  PageEntry &mutable_entry = impl_->pages[page_index];
  mutable_entry.width = probed_w;
  mutable_entry.height = probed_h;
  mutable_entry.size_known = (mutable_entry.width > 0 && mutable_entry.height > 0);
  if (!mutable_entry.size_known) return false;
  w = mutable_entry.width;
  h = mutable_entry.height;
  return true;
#else
  (void)w;
  (void)h;
  return false;
#endif
}

bool ZipImageReader::CurrentPageSize(int &w, int &h) const { return PageSize(CurrentPage(), w, h); }

bool ZipImageReader::RenderPageRGBA(int page_index, float scale, std::vector<unsigned char> &rgba, int &w, int &h,
                                    const std::atomic<bool> *cancel) {
  if (!IsOpen()) return false;
  page_index = std::clamp(page_index, 0, PageCount() - 1);
  scale = std::max(0.1f, scale);
  if (cancel && cancel->load()) return false;
#ifdef HAVE_LIBZIP
  PageEntry &entry = impl_->pages[page_index];
  const uint32_t perf_begin = SDL_GetTicks();
  std::vector<unsigned char> bytes;
  if (!ReadZipEntry(impl_->zip, entry.image_entry, bytes)) {
    runtime_log::Line("[zip_image] render read failed page=" + std::to_string(page_index) +
                      " entry=" + entry.image_entry);
    return false;
  }
  const uint32_t perf_read = SDL_GetTicks();
  if (cancel && cancel->load()) return false;
  int source_w = entry.width;
  int source_h = entry.height;
  if (!entry.size_known || source_w <= 0 || source_h <= 0) {
    ProbeImageSizeFromMemory(bytes.data(), bytes.size(), source_w, source_h);
  }
  const int target_w = source_w > 0 ? std::max(1, static_cast<int>(std::round(static_cast<float>(source_w) * scale))) : 0;
  const int target_h = source_h > 0 ? std::max(1, static_cast<int>(std::round(static_cast<float>(source_h) * scale))) : 0;
  SDL_Surface *surface = DecodeSurfaceFromMemoryFit(bytes.data(), bytes.size(), target_w, target_h);
  if (!surface) {
    runtime_log::Line("[zip_image] render decode failed page=" + std::to_string(page_index) +
                      " entry=" + entry.image_entry +
                      " bytes=" + std::to_string(bytes.size()));
    return false;
  }
  const uint32_t perf_decode = SDL_GetTicks();

  SDL_Surface *rgba_surface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(surface);
  if (!rgba_surface) {
    runtime_log::Line("[zip_image] render convert failed page=" + std::to_string(page_index) +
                      " entry=" + entry.image_entry +
                      " err=" + SDL_GetError());
    return false;
  }
  const uint32_t perf_convert = SDL_GetTicks();

  if (source_w > 0 && source_h > 0) {
    entry.width = source_w;
    entry.height = source_h;
  } else {
    entry.width = rgba_surface->w;
    entry.height = rgba_surface->h;
  }
  entry.size_known = (entry.width > 0 && entry.height > 0);

  w = target_w > 0 ? target_w : std::max(1, static_cast<int>(std::round(static_cast<float>(rgba_surface->w) * scale)));
  h = target_h > 0 ? target_h : std::max(1, static_cast<int>(std::round(static_cast<float>(rgba_surface->h) * scale)));

  SDL_Surface *scaled = rgba_surface;
  if (w != rgba_surface->w || h != rgba_surface->h) {
    if (cancel && cancel->load()) {
      SDL_FreeSurface(rgba_surface);
      return false;
    }
    scaled = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!scaled) {
      runtime_log::Line("[zip_image] render scaled surface failed page=" + std::to_string(page_index) +
                        " target=" + std::to_string(w) + "x" + std::to_string(h) +
                        " err=" + SDL_GetError());
      SDL_FreeSurface(rgba_surface);
      return false;
    }
    if (SDL_BlitScaled(rgba_surface, nullptr, scaled, nullptr) != 0) {
      runtime_log::Line("[zip_image] render blit scaled failed page=" + std::to_string(page_index) +
                        " src=" + std::to_string(rgba_surface->w) + "x" +
                        std::to_string(rgba_surface->h) +
                        " dst=" + std::to_string(w) + "x" + std::to_string(h) +
                        " err=" + SDL_GetError());
      SDL_FreeSurface(scaled);
      SDL_FreeSurface(rgba_surface);
      return false;
    }
    SDL_FreeSurface(rgba_surface);
  }
  const uint32_t perf_scale = SDL_GetTicks();

  rgba.assign(static_cast<size_t>(w * h * 4), 0);
  for (int y = 0; y < h; ++y) {
    if (cancel && cancel->load()) {
      SDL_FreeSurface(scaled);
      rgba.clear();
      return false;
    }
    const unsigned char *src_row = static_cast<const unsigned char *>(scaled->pixels) + y * scaled->pitch;
    unsigned char *dst_row = rgba.data() + static_cast<size_t>(y * w * 4);
    std::memcpy(dst_row, src_row, static_cast<size_t>(w * 4));
  }
  SDL_FreeSurface(scaled);
  if (ImagePerfLogEnabled()) {
    runtime_log::Line("[zip_image_perf] page=" + std::to_string(page_index) +
                      " bytes=" + std::to_string(bytes.size()) +
                      " source=" + std::to_string(entry.width) + "x" + std::to_string(entry.height) +
                      " target=" + std::to_string(w) + "x" + std::to_string(h) +
                      " read_ms=" + std::to_string(perf_read - perf_begin) +
                      " decode_ms=" + std::to_string(perf_decode - perf_read) +
                      " convert_ms=" + std::to_string(perf_convert - perf_decode) +
                      " scale_ms=" + std::to_string(perf_scale - perf_convert) +
                      " copy_ms=" + std::to_string(SDL_GetTicks() - perf_scale));
  }
  return true;
#else
  (void)rgba;
  (void)w;
  (void)h;
  return false;
#endif
}

bool ZipImageReader::RenderCurrentPageRGBA(float scale, std::vector<unsigned char> &rgba, int &w, int &h,
                                           const std::atomic<bool> *cancel) {
  return RenderPageRGBA(CurrentPage(), scale, rgba, w, h, cancel);
}
