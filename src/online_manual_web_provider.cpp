#include "online_manual_web_provider.h"

#include "online_source_transport.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>

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

std::string ExtractHtmlAttribute(const std::string &tag, const std::string &name) {
  static const std::regex attr_regex(R"(([A-Za-z0-9_:\.-]+)\s*=\s*(['"])(.*?)\2)",
                                    std::regex::icase);
  for (std::sregex_iterator it(tag.begin(), tag.end(), attr_regex), end; it != end; ++it) {
    if (ToLowerAscii((*it)[1].str()) == ToLowerAscii(name)) return DecodeXmlEntities((*it)[3].str());
  }
  return {};
}

std::string StripHtmlTags(std::string text) {
  text = std::regex_replace(text, std::regex(R"(<[^>]*>)"), "");
  text = std::regex_replace(text, std::regex(R"(_em)", std::regex::icase), "");
  return DecodeXmlEntities(Trim(text));
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

}  // namespace

ManualWebCatalogPage LoadManualWebCatalogPage(const std::string &url, const std::string &source_url) {
  ManualWebCatalogPage page;
  page.body = ManualWebFetch(url, source_url);
  if (page.body.empty()) return page;
  page.items = ParseManualWebCatalog(page.body, url);
  page.next_page_url = ParseManualWebNextPageUrl(page.body, url);
  return page;
}

std::vector<OnlineCatalogItem> ParseManualWebCatalog(const std::string &html, const std::string &base_url) {
  std::vector<OnlineCatalogItem> out;
  std::set<std::string> seen;
  size_t pos = 0;
  while ((pos = html.find("pic_box", pos)) != std::string::npos) {
    size_t block_begin = html.rfind("<li", pos);
    if (block_begin == std::string::npos) block_begin = html.rfind('<', pos);
    size_t block_end = html.find("</li>", pos);
    size_t block_close_len = 5;
    if (block_end == std::string::npos) {
      block_end = html.find("</div>", pos);
      block_close_len = 6;
    }
    if (block_begin == std::string::npos || block_end == std::string::npos) {
      pos += 7;
      continue;
    }
    const std::string block = html.substr(block_begin, block_end - block_begin + block_close_len);
    const size_t a_begin = block.find("<a");
    if (a_begin == std::string::npos) {
      pos = block_end + block_close_len;
      continue;
    }
    const size_t a_tag_end = block.find('>', a_begin);
    if (a_tag_end == std::string::npos) {
      pos = block_end + block_close_len;
      continue;
    }
    const std::string a_tag = block.substr(a_begin, a_tag_end - a_begin + 1);
    const std::string href = ResolveUrl(base_url, ExtractHtmlAttribute(a_tag, "href"));
    if (href.empty() || !seen.insert(href).second) {
      pos = block_end + block_close_len;
      continue;
    }
    std::string title = ExtractHtmlAttribute(a_tag, "title");
    if (title.empty()) {
      const size_t a_close = block.find("</a>", a_tag_end);
      if (a_close != std::string::npos) title = StripHtmlTags(block.substr(a_tag_end + 1, a_close - a_tag_end - 1));
    }
    if (title.empty()) title = std::filesystem::path(href).stem().string();

    std::string cover;
    const size_t img_begin = block.find("<img");
    if (img_begin != std::string::npos) {
      const size_t img_end = block.find('>', img_begin);
      if (img_end != std::string::npos) {
        const std::string img_tag = block.substr(img_begin, img_end - img_begin + 1);
        for (const char *name : {"data-original", "data-src", "src"}) {
          cover = ExtractHtmlAttribute(img_tag, name);
          if (!cover.empty()) break;
        }
      }
    }

    OnlineCatalogItem item;
    item.id = href;
    item.title = title;
    item.cover_url = ResolveUrl(base_url, cover);
    item.download_url = href;
    item.file_ext = ".zip";
    out.push_back(std::move(item));
    pos = block_end + block_close_len;
  }
  return out;
}

std::string ParseManualWebNextPageUrl(const std::string &html, const std::string &base_url) {
  std::smatch match;
  if (std::regex_search(html, match,
                        std::regex(R"(<span[^>]+class\s*=\s*['"]next['"][\s\S]*?<a[^>]+href\s*=\s*['"]([^'"]+)['"])",
                                   std::regex::icase))) {
    return ResolveUrl(base_url, DecodeXmlEntities(match[1].str()));
  }
  return {};
}

std::string ResolveManualWebDownloadUrl(const std::string &detail_url, const std::string &title,
                                        const std::string &source_url) {
  return ManualWebResolveDownload(detail_url, title, source_url);
}

bool DownloadManualWebFile(const std::string &url, const std::filesystem::path &output_path,
                           const std::string &referer) {
  return ManualWebDownload(url, output_path, referer);
}
