#include "zip_image_cover_cache.h"

#include "image_decode.h"
#include "runtime_log.h"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include "filesystem_compat.h"
#include <sstream>
#include <string>
#include <vector>

#ifdef HAVE_LIBZIP
#include <zip.h>
#endif

namespace {
struct FileCacheMeta {
  uintmax_t size = 0;
  long long mtime = 0;
};

FileCacheMeta ReadFileCacheMeta(const std::string &doc_path) {
  FileCacheMeta meta;
  std::error_code ec;
  const std::filesystem::path path(doc_path);
  meta.size = std::filesystem::file_size(path, ec);
  if (ec) meta.size = 0;
  ec.clear();
  const auto mtime_raw = std::filesystem::last_write_time(path, ec);
  meta.mtime = ec ? 0LL : static_cast<long long>(mtime_raw.time_since_epoch().count());
  return meta;
}

std::string MakeZipImageCoverCacheKey(const std::string &doc_path, const FileCacheMeta &meta,
                                      int cover_w, int cover_h,
                                      const ZipImageCoverTextureDeps &deps) {
  return deps.normalize_path_key(doc_path) + "|" + std::to_string(meta.size) + "|" +
         std::to_string(meta.mtime) + "|" + std::to_string(cover_w) + "x" +
         std::to_string(cover_h) + "|zip-image-cover-v1";
}

std::filesystem::path GetZipImageCoverCacheFile(const std::string &doc_path, const FileCacheMeta &meta,
                                                const ZipImageCoverTextureDeps &deps) {
  const std::string cache_key =
      MakeZipImageCoverCacheKey(doc_path, meta, deps.cover_w, deps.cover_h, deps);
  const size_t hash_value = std::hash<std::string>{}(cache_key);
  std::ostringstream oss;
  oss << std::hex << hash_value << ".bmp";
  return deps.cover_thumb_cache_dir / oss.str();
}

SDL_Texture *LoadCachedZipImageCoverTexture(const std::string &doc_path, const FileCacheMeta &meta,
                                            ZipImageCoverTextureDeps &deps) {
  const std::filesystem::path cache_file = GetZipImageCoverCacheFile(doc_path, meta, deps);
  std::error_code ec;
  if (!std::filesystem::exists(cache_file, ec) || ec) return nullptr;
  SDL_Surface *cover_surface = deps.load_surface_from_file(cache_file.string());
  if (!cover_surface) return nullptr;
  SDL_Texture *normalized =
      deps.create_normalized_cover_texture(deps.renderer, cover_surface, deps.cover_w, deps.cover_h,
                                           static_cast<float>(deps.cover_w) / static_cast<float>(deps.cover_h));
  if (!normalized) normalized = deps.create_texture_from_surface(deps.renderer, cover_surface);
  SDL_FreeSurface(cover_surface);
  if (normalized) {
    deps.remember_texture_size(normalized, deps.cover_w, deps.cover_h);
  }
  return normalized;
}

void SaveZipImageCoverCacheToDisk(const std::string &doc_path, const FileCacheMeta &meta,
                                  SDL_Surface *cover_surface,
                                  const ZipImageCoverTextureDeps &deps) {
  if (!cover_surface) return;
  std::error_code ec;
  std::filesystem::create_directories(deps.cover_thumb_cache_dir, ec);
  const std::filesystem::path cache_file = GetZipImageCoverCacheFile(doc_path, meta, deps);
  SDL_SaveBMP(cover_surface, cache_file.string().c_str());
}

#ifdef HAVE_LIBZIP
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

bool ReadZipEntryByIndex(zip_t *za, zip_uint64_t index, std::vector<unsigned char> &out) {
  zip_stat_t st{};
  if (zip_stat_index(za, index, 0, &st) != 0) return false;
  if (st.size > static_cast<zip_uint64_t>(128 * 1024 * 1024)) return false;
  zip_file_t *zf = zip_fopen_index(za, index, 0);
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
#endif

bool ExtractFirstZipImageBytes(const std::string &doc_path, std::vector<unsigned char> &out) {
#ifdef HAVE_LIBZIP
  int zerr = 0;
  zip_t *za = zip_open(doc_path.c_str(), ZIP_RDONLY, &zerr);
  if (!za) {
    runtime_log::Line("[zip_image_cover] zip_open failed path=" + doc_path + " zerr=" + std::to_string(zerr));
    return false;
  }
  const zip_int64_t count = zip_get_num_entries(za, 0);
  for (zip_int64_t i = 0; i < count; ++i) {
    const char *raw_name = zip_get_name(za, static_cast<zip_uint64_t>(i), 0);
    if (!raw_name) continue;
    const std::string entry_name = NormalizeZipPath(raw_name);
    if (IsIgnoredEntryPath(entry_name) || !IsLikelyImagePath(entry_name)) continue;
    const bool ok = ReadZipEntryByIndex(za, static_cast<zip_uint64_t>(i), out);
    zip_close(za);
    if (!ok) {
      runtime_log::Line("[zip_image_cover] first image read failed path=" + doc_path +
                        " entry=" + entry_name);
    }
    return ok;
  }
  zip_close(za);
  runtime_log::Line("[zip_image_cover] no image entries path=" + doc_path);
  return false;
#else
  (void)doc_path;
  (void)out;
  return false;
#endif
}
}

SDL_Texture *CreateZipImageFirstImageCoverTexture(const std::string &doc_path,
                                                  ZipImageCoverTextureDeps &deps) {
  const FileCacheMeta cache_meta = ReadFileCacheMeta(doc_path);
  if (SDL_Texture *cached = LoadCachedZipImageCoverTexture(doc_path, cache_meta, deps)) {
    return cached;
  }

  std::vector<unsigned char> cover_bytes;
  if (!ExtractFirstZipImageBytes(doc_path, cover_bytes)) return nullptr;

  SDL_Surface *cover_surface =
      deps.load_surface_from_memory(cover_bytes.data(), cover_bytes.size());
  if (!cover_surface) cover_surface = DecodeSurfaceFromMemory(cover_bytes.data(), cover_bytes.size());
  if (!cover_surface) {
    runtime_log::Line("[zip_image_cover] decode surface failed path=" + doc_path);
    return nullptr;
  }
  SDL_Texture *normalized =
      deps.create_normalized_cover_texture(deps.renderer, cover_surface, deps.cover_w, deps.cover_h,
                                           static_cast<float>(deps.cover_w) / static_cast<float>(deps.cover_h));
  if (!normalized) normalized = deps.create_texture_from_surface(deps.renderer, cover_surface);
  if (normalized) {
    deps.remember_texture_size(normalized, deps.cover_w, deps.cover_h);
    SaveZipImageCoverCacheToDisk(doc_path, cache_meta, cover_surface, deps);
  }
  SDL_FreeSurface(cover_surface);
  return normalized;
}

bool HasCachedZipImageCoverOnDisk(const std::string &doc_path, ZipImageCoverTextureDeps &deps) {
  const FileCacheMeta cache_meta = ReadFileCacheMeta(doc_path);
  std::error_code ec;
  const std::filesystem::path cache_file = GetZipImageCoverCacheFile(doc_path, cache_meta, deps);
  return std::filesystem::exists(cache_file, ec) && !ec;
}
