#include "online_opds_provider.h"

#include "filesystem_compat.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {
std::string Trim(std::string text) {
  auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  while (!text.empty() && is_space(static_cast<unsigned char>(text.front()))) text.erase(text.begin());
  while (!text.empty() && is_space(static_cast<unsigned char>(text.back()))) text.pop_back();
  return text;
}

bool StartsWith(const std::string &text, const std::string &prefix) {
  return text.rfind(prefix, 0) == 0;
}

std::string ToLowerAscii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return text;
}

std::string DecodeXmlEntities(std::string text) {
  auto replace_all = [&](const std::string &from, const std::string &to) {
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
      text.replace(pos, from.size(), to);
      pos += to.size();
    }
  };
  replace_all("&amp;", "&");
  replace_all("&lt;", "<");
  replace_all("&gt;", ">");
  replace_all("&quot;", "\"");
  replace_all("&apos;", "'");
  return text;
}

std::string ExtractBetween(const std::string &text, const std::string &open_tag, const std::string &close_tag) {
  const size_t begin = text.find(open_tag);
  if (begin == std::string::npos) return {};
  const size_t content_begin = text.find('>', begin);
  if (content_begin == std::string::npos) return {};
  const size_t end = text.find(close_tag, content_begin + 1);
  if (end == std::string::npos) return {};
  return DecodeXmlEntities(Trim(text.substr(content_begin + 1, end - content_begin - 1)));
}

std::string ExtractXmlAttribute(const std::string &tag, const std::string &name) {
  const std::string prefix = name + "=";
  const size_t pos = tag.find(prefix);
  if (pos == std::string::npos) return {};
  const size_t quote_pos = pos + prefix.size();
  if (quote_pos >= tag.size() || (tag[quote_pos] != '"' && tag[quote_pos] != '\'')) return {};
  const char quote = tag[quote_pos];
  const size_t end = tag.find(quote, quote_pos + 1);
  if (end == std::string::npos) return {};
  return DecodeXmlEntities(tag.substr(quote_pos + 1, end - quote_pos - 1));
}

std::vector<std::string> ExtractLinkTags(const std::string &entry_text) {
  std::vector<std::string> tags;
  size_t pos = 0;
  while ((pos = entry_text.find("<link", pos)) != std::string::npos) {
    const size_t end = entry_text.find('>', pos);
    if (end == std::string::npos) break;
    tags.push_back(entry_text.substr(pos, end - pos + 1));
    pos = end + 1;
  }
  return tags;
}

bool LooksLikeAcquisitionRel(const std::string &rel) {
  const std::string lower = ToLowerAscii(rel);
  return lower.find("acquisition") != std::string::npos || lower == "http://opds-spec.org/acquisition" ||
         lower == "enclosure";
}

bool LooksLikeImageRel(const std::string &rel, const std::string &type) {
  const std::string lower_rel = ToLowerAscii(rel);
  const std::string lower_type = ToLowerAscii(type);
  return lower_rel.find("image") != std::string::npos || lower_type.find("image/") != std::string::npos;
}

std::string ResolveUrl(const std::string &base_url, const std::string &href) {
  if (href.empty()) return {};
  const std::string lower = ToLowerAscii(href);
  if (StartsWith(lower, "http://") || StartsWith(lower, "https://") || StartsWith(lower, "file://")) return href;
  if (StartsWith(href, "//")) {
    const size_t scheme = base_url.find("://");
    return scheme == std::string::npos ? std::string("http:") + href : base_url.substr(0, scheme) + ":" + href;
  }
  const size_t scheme = base_url.find("://");
  if (scheme == std::string::npos) return href;
  const size_t authority_begin = scheme + 3;
  const size_t path_begin = base_url.find('/', authority_begin);
  const std::string origin = path_begin == std::string::npos ? base_url : base_url.substr(0, path_begin);
  if (!href.empty() && href.front() == '/') return origin + href;
  std::string dir = path_begin == std::string::npos ? origin + "/" : base_url.substr(0, base_url.find_last_of('/') + 1);
  return dir + href;
}

std::string ExtensionFromUrlOrType(const std::string &url, const std::string &type) {
  std::string lower_type = ToLowerAscii(type);
  if (lower_type.find("epub") != std::string::npos) return ".epub";
  if (lower_type.find("pdf") != std::string::npos) return ".pdf";
  if (lower_type.find("zip") != std::string::npos) return ".zip";
  if (lower_type.find("cbz") != std::string::npos || lower_type.find("comicbook") != std::string::npos) return ".cbz";
  try {
    std::string ext = std::filesystem::path(url).extension().string();
    const size_t query = ext.find_first_of("?#");
    if (query != std::string::npos) ext = ext.substr(0, query);
    ext = ToLowerAscii(ext);
    if (ext == ".epub" || ext == ".pdf" || ext == ".zip" || ext == ".cbz") return ext;
  } catch (...) {
  }
  return ".cbz";
}

std::string HashText(const std::string &text) {
  std::ostringstream oss;
  oss << std::hex << std::hash<std::string>{}(text);
  return oss.str();
}
}  // namespace

std::vector<OnlineCatalogItem> ParseOpdsCatalog(const std::string &xml, const std::string &base_url) {
  std::vector<OnlineCatalogItem> out;
  size_t pos = 0;
  while ((pos = xml.find("<entry", pos)) != std::string::npos) {
    const size_t entry_open_end = xml.find('>', pos);
    const size_t entry_end = xml.find("</entry>", entry_open_end == std::string::npos ? pos : entry_open_end);
    if (entry_open_end == std::string::npos || entry_end == std::string::npos) break;
    const std::string entry = xml.substr(pos, entry_end - pos + 8);
    pos = entry_end + 8;

    OnlineCatalogItem item;
    item.title = ExtractBetween(entry, "<title", "</title>");
    const std::string id = ExtractBetween(entry, "<id", "</id>");
    item.id = id.empty() ? HashText(entry) : id;
    for (const std::string &tag : ExtractLinkTags(entry)) {
      const std::string href = ResolveUrl(base_url, ExtractXmlAttribute(tag, "href"));
      const std::string rel = ExtractXmlAttribute(tag, "rel");
      const std::string type = ExtractXmlAttribute(tag, "type");
      if (href.empty()) continue;
      if (item.cover_url.empty() && LooksLikeImageRel(rel, type)) item.cover_url = href;
      if (item.download_url.empty() && LooksLikeAcquisitionRel(rel)) {
        item.download_url = href;
        item.file_ext = ExtensionFromUrlOrType(href, type);
      }
    }
    if (!item.title.empty() && !item.download_url.empty()) out.push_back(std::move(item));
  }
  return out;
}
