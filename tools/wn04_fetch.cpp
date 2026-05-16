#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <chrono>
#if __has_include(<filesystem>)
#include <filesystem>
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
#else
#error "wn04_fetch requires filesystem or experimental/filesystem"
#endif
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>
#include <sys/wait.h>
#include <vector>

namespace {

#if __has_include(<filesystem>)
namespace fs = std::filesystem;
#else
namespace fs = std::experimental::filesystem;
#endif

struct CommandResult {
  std::string output;
  int exit_code = -1;
};

std::string ShellQuote(const std::string &value) {
  std::string out = "'";
  for (char c : value) {
    if (c == '\'') out += "'\\''";
    else out.push_back(c);
  }
  out.push_back('\'');
  return out;
}

int DecodeStatus(int status) {
  if (status == -1) return -1;
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
  return status;
}

CommandResult RunCapture(const std::vector<std::string> &args) {
  std::ostringstream command;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i) command << ' ';
    command << ShellQuote(args[i]);
  }
  command << " 2>&1";

  FILE *pipe = popen(command.str().c_str(), "r");
  if (!pipe) return {};

  CommandResult result;
  char buffer[4096] = {};
  while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) result.output += buffer;
  result.exit_code = DecodeStatus(pclose(pipe));
  return result;
}

int RunStatus(const std::vector<std::string> &args) {
  std::ostringstream command;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i) command << ' ';
    command << ShellQuote(args[i]);
  }
  return DecodeStatus(std::system(command.str().c_str()));
}

std::string Trim(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
  return value;
}

std::string ResolveUrl(const std::string &base_url, const std::string &href) {
  if (href.empty()) return {};
  if (href.rfind("http://", 0) == 0 || href.rfind("https://", 0) == 0) return href;
  if (href.rfind("//", 0) == 0) {
    const size_t scheme = base_url.find("://");
    return scheme == std::string::npos ? "https:" + href : base_url.substr(0, scheme) + ":" + href;
  }
  const size_t scheme = base_url.find("://");
  if (scheme == std::string::npos) return href;
  const size_t host_begin = scheme + 3;
  const size_t path_begin = base_url.find('/', host_begin);
  const std::string origin = path_begin == std::string::npos ? base_url : base_url.substr(0, path_begin);
  if (!href.empty() && href.front() == '/') return origin + href;
  const size_t slash = base_url.find_last_of('/');
  return (slash == std::string::npos ? origin + "/" : base_url.substr(0, slash + 1)) + href;
}

std::string OriginFromUrl(const std::string &url) {
  const size_t scheme = url.find("://");
  if (scheme == std::string::npos) return "https://www.wn04.cfd";
  const size_t host_begin = scheme + 3;
  const size_t path_begin = url.find('/', host_begin);
  return path_begin == std::string::npos ? url : url.substr(0, path_begin);
}

std::string JsonEscape(const std::string &value) {
  std::string out;
  for (char c : value) {
    if (c == '\\') out += "\\\\";
    else if (c == '"') out += "\\\"";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else out.push_back(c);
  }
  return out;
}

std::string ExtractJsonString(const std::string &json, const std::string &key) {
  const std::string needle = "\"" + key + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) return {};
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos) return {};
  pos = json.find('"', pos + 1);
  if (pos == std::string::npos) return {};
  std::string out;
  bool escape = false;
  for (++pos; pos < json.size(); ++pos) {
    const char c = json[pos];
    if (escape) {
      out.push_back(c);
      escape = false;
    } else if (c == '\\') {
      escape = true;
    } else if (c == '"') {
      break;
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::string ExtractBackupDownloadUrl(const std::string &html, const std::string &base_url) {
  std::smatch match;
  const std::regex href_pattern(
      R"(<a[^>]+href\s*=\s*(['"])([^'"]*wn01\.download/[^'"]+\.zip(?:\?[^'"]*)?)\1)",
      std::regex::icase);
  if (std::regex_search(html, match, href_pattern)) return ResolveUrl(base_url, match[2].str());
  const std::regex any_pattern(
      R"(['"]((?://|https?://)?[^'"]*wn01\.download/[^'"]+\.zip(?:\?[^'"]*)?)['"])",
      std::regex::icase);
  if (std::regex_search(html, match, any_pattern)) return ResolveUrl(base_url, match[1].str());
  return {};
}

std::string NormalizeZipDownloadUrl(std::string url) {
  const size_t zip = url.find(".zip");
  if (zip == std::string::npos) return url;
  const size_t after_zip = zip + 4;
  const size_t query = url.find('?', after_zip);
  if (query == std::string::npos) return url;
  const std::string query_text = url.substr(query + 1);
  if (url.find("wn01.download") != std::string::npos &&
      (query_text.rfind("n=", 0) == 0 || query_text.find_first_of(" \t\r\n") != std::string::npos)) {
    return url.substr(0, after_zip);
  }
  return url;
}

std::string SafeFilename(std::string name) {
  name = Trim(name);
  if (name.empty()) return "online_book";
  for (char &c : name) {
    const unsigned char ch = static_cast<unsigned char>(c);
    if (ch < 32 || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
        c == '>' || c == '|') {
      c = '_';
    }
  }
  while (!name.empty() && (name.back() == ' ' || name.back() == '.')) name.pop_back();
  return name.empty() ? "online_book" : name;
}

fs::path ExecutableDir(const char *argv0) {
  std::error_code ec;
  fs::path self = fs::canonical(argv0, ec);
  if (ec) {
    ec.clear();
    const fs::path current = fs::current_path(ec);
    self = ec ? fs::path(argv0) : current / argv0;
  }
  if (self.empty()) return fs::current_path();
  return self.parent_path();
}

std::string CurlPath(const char *argv0) {
  if (const char *env = std::getenv("ROCREADER_MANUAL_WEB_CURL"); env && *env) return env;
  if (const char *env = std::getenv("WN04_CURL_IMPERSONATE"); env && *env) return env;
  if (const char *env = std::getenv("WN04_CURL_CHROME"); env && *env) return env;
  const fs::path dir = ExecutableDir(argv0);
  const std::vector<fs::path> candidates = {
      dir / "curl-impersonate",
      dir / "curl_chrome",
      dir / "curl-impersonate-chrome",
      dir.parent_path() / "bin" / "curl-impersonate",
      dir.parent_path() / "bin" / "curl_chrome",
      fs::current_path() / "bin" / "curl-impersonate",
      fs::current_path() / "bin" / "curl_chrome",
  };
  std::error_code ec;
  for (const auto &candidate : candidates) {
    if (fs::exists(candidate, ec) && !ec) return candidate.string();
  }
  return "curl-impersonate";
}

std::string EnvValue(const char *name) {
  const char *value = std::getenv(name);
  return value && *value ? value : "";
}

std::vector<std::string> CurlBaseArgs(const std::string &curl, const std::string &referer) {
  const std::string connect_time =
      EnvValue("WN04_FETCH_CONNECT_TIME").empty() ? "8" : EnvValue("WN04_FETCH_CONNECT_TIME");
  const std::string max_time = EnvValue("WN04_FETCH_MAX_TIME").empty() ? "25" : EnvValue("WN04_FETCH_MAX_TIME");
  std::vector<std::string> args = {curl, "-LfsS", "--globoff", "--connect-timeout", connect_time,
                                  "--max-time", max_time};
  const std::string mode = EnvValue("WN04_FETCH_HEADER_MODE");
  if (mode.empty() || mode == "chrome120-h3" || mode == "success") {
    args.push_back("--impersonate");
    args.push_back("chrome120");
    args.push_back("--http3-only");
    args.push_back("--compressed");
  } else if (mode == "legacy") {
    args.push_back("--compressed");
    args.push_back("--http2");
    args.push_back("--tlsv1.3");
    args.push_back("-H");
    args.push_back(
        "User-Agent: Mozilla/5.0 (X11; Linux aarch64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 "
        "Safari/537.36");
    args.push_back("-H");
    args.push_back("Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8");
    args.push_back("-H");
    args.push_back("Accept-Language: zh-CN,zh;q=0.9,en;q=0.8");
  } else if (mode == "minimal") {
    args.push_back("--compressed");
    args.push_back("--http2");
  }
  if (!referer.empty()) {
    args.push_back("-H");
    args.push_back("Referer: " + referer);
  }
  return args;
}

void AddCookieJar(std::vector<std::string> &args, const std::string &cookie_jar) {
  if (cookie_jar.empty()) return;
  args.push_back("-b");
  args.push_back(cookie_jar);
  args.push_back("-c");
  args.push_back(cookie_jar);
}

std::vector<std::string> CurlApiArgs(const std::string &curl, const std::string &referer) {
  const std::string max_time = EnvValue("WN04_API_MAX_TIME").empty() ? "12" : EnvValue("WN04_API_MAX_TIME");
  std::vector<std::string> args = {curl, "-LfsS", "--globoff", "--connect-timeout", "8", "--max-time", max_time,
                                  "--impersonate", "chrome120", "--http2", "--compressed"};
  if (!referer.empty()) {
    args.push_back("-H");
    args.push_back("Referer: " + referer);
  }
  return args;
}

std::vector<std::string> CurlDownloadArgs(const std::string &curl, const std::string &referer) {
  const std::string max_time = EnvValue("WN04_DOWNLOAD_MAX_TIME").empty() ? "1800" : EnvValue("WN04_DOWNLOAD_MAX_TIME");
  std::vector<std::string> args = {curl, "-LfsS", "--globoff", "--connect-timeout", "30", "--max-time", max_time,
                                  "--retry", "2", "--retry-delay", "2"};
  const std::string mode = EnvValue("WN04_DOWNLOAD_HEADER_MODE");
  if (mode.empty() || mode == "chrome120-h2" || mode == "success") {
    args.push_back("--impersonate");
    args.push_back("chrome120");
    args.push_back("--http2");
    args.push_back("--compressed");
  } else if (mode == "chrome120-h3" || mode == "h3") {
    args.push_back("--impersonate");
    args.push_back("chrome120");
    args.push_back("--http3-only");
    args.push_back("--compressed");
  } else if (mode == "minimal") {
    args.push_back("--compressed");
    args.push_back("--http2");
  }
  if (!referer.empty()) {
    args.push_back("-H");
    args.push_back("Referer: " + referer);
  }
  args.push_back("-w");
  args.push_back("WN04_DOWNLOAD_RESULT http=%{http_code} remote=%{remote_ip} local=%{local_ip} size=%{size_download} time=%{time_total} speed=%{speed_download}\\n");
  return args;
}

bool IsCoverUrl(const std::string &url) {
  return url.find("wnacgimg") != std::string::npos;
}

std::vector<std::string> CurlCoverArgs(const std::string &curl, const std::string &referer) {
  const std::string connect_time =
      EnvValue("WN04_COVER_CONNECT_TIME").empty() ? "3" : EnvValue("WN04_COVER_CONNECT_TIME");
  const std::string max_time = EnvValue("WN04_COVER_MAX_TIME").empty() ? "5" : EnvValue("WN04_COVER_MAX_TIME");
  const std::string retry_count = EnvValue("WN04_COVER_RETRY").empty() ? "0" : EnvValue("WN04_COVER_RETRY");
  std::vector<std::string> args = {curl, "-LfsS", "--globoff", "--connect-timeout", connect_time,
                                  "--max-time", max_time, "--retry", retry_count, "--retry-delay", "1"};
  const std::string mode = EnvValue("WN04_COVER_HEADER_MODE");
  if (mode.empty() || mode == "chrome120-h2" || mode == "success") {
    args.push_back("--impersonate");
    args.push_back("chrome120");
    args.push_back("--http2");
    args.push_back("--compressed");
  } else if (mode == "chrome120-h3" || mode == "h3") {
    args.push_back("--impersonate");
    args.push_back("chrome120");
    args.push_back("--http3-only");
    args.push_back("--compressed");
  } else if (mode == "minimal") {
    args.push_back("--compressed");
    args.push_back("--http2");
  }
  if (!referer.empty()) {
    args.push_back("-H");
    args.push_back("Referer: " + referer);
  }
  args.push_back("-w");
  args.push_back("WN04_COVER_RESULT http=%{http_code} remote=%{remote_ip} local=%{local_ip} size=%{size_download} time=%{time_total} speed=%{speed_download}\\n");
  return args;
}

CommandResult Fetch(const std::string &curl, const std::string &url, const std::string &referer,
                    const std::string &cookie_jar = "") {
  std::vector<std::string> args = CurlBaseArgs(curl, referer);
  AddCookieJar(args, cookie_jar);
  args.push_back(url);
  return RunCapture(args);
}

int Download(const std::string &curl, const std::string &url, const std::string &output, const std::string &referer) {
  std::vector<std::string> args = IsCoverUrl(url) ? CurlCoverArgs(curl, referer) : CurlDownloadArgs(curl, referer);
  args.push_back("-o");
  args.push_back(output);
  args.push_back(url);
  CommandResult result = RunCapture(args);
  if (result.exit_code == 0) {
    std::error_code ec;
    if (fs::exists(output, ec) && !ec && fs::file_size(output, ec) > 0 && !ec) {
      return 0;
    }
    std::cerr << "{\"error\":\"download_output_missing\",\"detail\":\"" << JsonEscape(result.output) << "\"}\n";
    return 8;
  }
  std::cerr << "{\"error\":\"download_failed\",\"detail\":\"" << JsonEscape(result.output) << "\"}\n";
  return result.exit_code;
}

int Size(const std::string &curl, const std::string &url, const std::string &referer) {
  std::vector<std::string> args = IsCoverUrl(url) ? CurlCoverArgs(curl, referer) : CurlDownloadArgs(curl, referer);
  args.push_back("-I");
  args.push_back("-o");
  args.push_back("/dev/null");
  args.push_back("-w");
  args.push_back("WN04_SIZE_RESULT http=%{http_code} size=%{size_download} content_length=%{content_length_download}\\n");
  args.push_back(url);
  CommandResult result = RunCapture(args);
  if (result.exit_code != 0) {
    std::cerr << "{\"error\":\"size_probe_failed\",\"detail\":\"" << JsonEscape(result.output) << "\"}\n";
    return result.exit_code;
  }
  std::smatch match;
  std::regex content_length_pattern(R"(content_length=([0-9]+))", std::regex::icase);
  if (std::regex_search(result.output, match, content_length_pattern) && match[1].str() != "0") {
    std::cout << "{\"size\":\"" << match[1].str() << "\"}\n";
    return 0;
  }
  std::regex header_pattern(R"((?:^|\r?\n)content-length:\s*([0-9]+))", std::regex::icase);
  std::string size;
  for (std::sregex_iterator it(result.output.begin(), result.output.end(), header_pattern), end; it != end; ++it) {
    size = (*it)[1].str();
  }
  if (!size.empty() && size != "0") {
    std::cout << "{\"size\":\"" << size << "\"}\n";
    return 0;
  }
  std::cerr << "{\"error\":\"content_length_missing\",\"detail\":\"" << JsonEscape(result.output) << "\"}\n";
  return 9;
}

int Resolve(const std::string &curl, const std::string &detail_url, const std::string &title,
            const std::string &source_url) {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  fs::path cookie_path =
      fs::temp_directory_path() / ("wn04_fetch_cookies_" + std::to_string(now) + ".txt");
  const std::string cookie_jar = cookie_path.string();

  CommandResult detail = Fetch(curl, detail_url, source_url, cookie_jar);
  if (detail.exit_code != 0 || detail.output.empty()) {
    std::error_code ec;
    fs::remove(cookie_path, ec);
    std::cerr << "{\"error\":\"detail_fetch_failed\",\"detail\":\"" << JsonEscape(detail.output) << "\"}\n";
    return 2;
  }

  std::smatch match;
  if (!std::regex_search(detail.output, match,
                         std::regex(R"(<a[^>]+href\s*=\s*(['"])([^'"]*/download-index-aid-[^'"]*)\1)",
                                    std::regex::icase))) {
    std::error_code ec;
    fs::remove(cookie_path, ec);
    std::cerr << "{\"error\":\"download_landing_link_not_found\"}\n";
    return 3;
  }
  const std::string landing_url = ResolveUrl(detail_url, match[2].str());
  CommandResult landing = Fetch(curl, landing_url, detail_url, cookie_jar);
  if (landing.exit_code != 0 || landing.output.empty()) {
    std::error_code ec;
    fs::remove(cookie_path, ec);
    std::cerr << "{\"error\":\"landing_fetch_failed\",\"detail\":\"" << JsonEscape(landing.output) << "\"}\n";
    return 4;
  }
  if (const std::string backup_url = ExtractBackupDownloadUrl(landing.output, landing_url); !backup_url.empty()) {
    std::error_code ec;
    fs::remove(cookie_path, ec);
    std::cout << "{\"url\":\"" << JsonEscape(NormalizeZipDownloadUrl(backup_url)) << "\"}\n";
    return 0;
  }
  if (!std::regex_search(landing.output, match, std::regex(R"(['"]((?:down/)[^'"]+\.zip)['"])", std::regex::icase))) {
    std::error_code ec;
    fs::remove(cookie_path, ec);
    std::cerr << "{\"error\":\"download_key_not_found\"}\n";
    return 5;
  }

  const std::string api = "https://d1.wcdn.date/api/generate-link";
  const std::string body =
      "{\"file_key\":\"" + JsonEscape(match[1].str()) + "\",\"file_name\":\"" + JsonEscape(SafeFilename(title)) +
      ".zip\"}";
  std::vector<std::string> args = CurlApiArgs(curl, landing_url);
  AddCookieJar(args, cookie_jar);
  args.push_back("-X");
  args.push_back("POST");
  args.push_back("-H");
  args.push_back("Content-Type: application/json");
  args.push_back("-H");
  args.push_back("Origin: " + OriginFromUrl(source_url.empty() ? detail_url : source_url));
  args.push_back("--data");
  args.push_back(body);
  args.push_back(api);
  CommandResult api_result = RunCapture(args);
  std::error_code ec;
  fs::remove(cookie_path, ec);
  if (api_result.exit_code != 0 || api_result.output.empty()) {
    std::cerr << "{\"error\":\"generate_link_failed\",\"detail\":\"" << JsonEscape(api_result.output) << "\"}\n";
    return 6;
  }
  const std::string real_url = ExtractJsonString(api_result.output, "url");
  if (real_url.empty()) {
    std::cerr << "{\"error\":\"real_download_url_not_found\",\"detail\":\"" << JsonEscape(api_result.output)
              << "\"}\n";
    return 7;
  }
  std::cout << "{\"url\":\"" << JsonEscape(NormalizeZipDownloadUrl(real_url)) << "\"}\n";
  return 0;
}

void Usage() {
  std::cerr << "usage: wn04_fetch fetch URL [REFERER]\n"
            << "       wn04_fetch download URL OUTPUT [REFERER]\n"
            << "       wn04_fetch size URL [REFERER]\n"
            << "       wn04_fetch resolve DETAIL_URL TITLE SOURCE_URL\n";
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 3) {
    Usage();
    return 64;
  }

  const std::string mode = argv[1];
  const std::string curl = CurlPath(argv[0]);

  if (mode == "fetch") {
    const CommandResult result = Fetch(curl, argv[2], argc > 3 ? argv[3] : "");
    if (result.exit_code != 0) {
      std::cerr << result.output;
      return result.exit_code;
    }
    std::cout << result.output;
    return 0;
  }
  if (mode == "download") {
    if (argc < 4) {
      Usage();
      return 64;
    }
    return Download(curl, argv[2], argv[3], argc > 4 ? argv[4] : "");
  }
  if (mode == "size") {
    return Size(curl, argv[2], argc > 3 ? argv[3] : "");
  }
  if (mode == "resolve") {
    if (argc < 5) {
      Usage();
      return 64;
    }
    return Resolve(curl, argv[2], argv[3], argv[4]);
  }

  Usage();
  return 64;
}
