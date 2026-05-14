#include "online_source_transport.h"

#include "runtime_log.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <regex>
#include <sstream>
#include <system_error>
#if !defined(_WIN32)
#include <sys/wait.h>
#endif

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
struct CommandCaptureResult {
  std::string output;
  int exit_code = -1;
};

int RunCommand(const std::string &command);

std::string Trim(std::string text) {
  const auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
  while (!text.empty() && is_space(static_cast<unsigned char>(text.front()))) text.erase(text.begin());
  while (!text.empty() && is_space(static_cast<unsigned char>(text.back()))) text.pop_back();
  return text;
}

std::string EscapeForPosix(const std::string &value) {
  std::string out = "'";
  for (char c : value) {
    if (c == '\'') out += "'\\''";
    else out.push_back(c);
  }
  out.push_back('\'');
  return out;
}

std::string QuoteWinArg(const std::string &value) {
  if (value.empty()) return "\"\"";
  std::string out = "\"";
  for (char c : value) {
    if (c == '"') out += "\"\"";
    else out.push_back(c);
  }
  out.push_back('"');
  return out;
}

std::string EscapeForPowerShell(const std::string &value) {
  std::string out = "'";
  for (char c : value) {
    if (c == '\'') out += "''";
    else out.push_back(c);
  }
  out.push_back('\'');
  return out;
}

std::string EscapeForCmdDoubleQuote(const std::string &value) {
  std::string escaped = "\"";
  for (char ch : value) {
    if (ch == '"') escaped += "\\\"";
    else escaped.push_back(ch);
  }
  escaped += "\"";
  return escaped;
}

std::string RunCommandCapture(const std::string &command) {
#if defined(_WIN32)
  SECURITY_ATTRIBUTES security_attributes{};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.bInheritHandle = TRUE;
  HANDLE read_pipe = nullptr;
  HANDLE write_pipe = nullptr;
  if (!CreatePipe(&read_pipe, &write_pipe, &security_attributes, 0)) return {};
  SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOA startup_info{};
  startup_info.cb = sizeof(startup_info);
  startup_info.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
  startup_info.wShowWindow = SW_HIDE;
  startup_info.hStdOutput = write_pipe;
  startup_info.hStdError = write_pipe;
  PROCESS_INFORMATION process_info{};
  std::string command_line = "cmd.exe /C " + command;
  const BOOL created = CreateProcessA(nullptr, command_line.data(), nullptr, nullptr, TRUE,
                                      CREATE_NO_WINDOW, nullptr, nullptr, &startup_info, &process_info);
  CloseHandle(write_pipe);
  if (!created) {
    CloseHandle(read_pipe);
    return {};
  }

  std::string output;
  char buffer[512] = {};
  DWORD bytes_read = 0;
  while (ReadFile(read_pipe, buffer, sizeof(buffer), &bytes_read, nullptr) && bytes_read > 0) {
    output.append(buffer, buffer + bytes_read);
  }
  WaitForSingleObject(process_info.hProcess, INFINITE);
  CloseHandle(process_info.hThread);
  CloseHandle(process_info.hProcess);
  CloseHandle(read_pipe);
  return output;
#else
  FILE *pipe = popen(command.c_str(), "r");
  if (!pipe) return {};
  std::string output;
  char buffer[512] = {};
  while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) output += buffer;
  pclose(pipe);
  return output;
#endif
}

#if !defined(_WIN32)
int DecodeProcessStatus(int status) {
  if (status == -1) return -1;
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
  return status;
}

CommandCaptureResult RunCommandCaptureWithStatus(const std::string &command) {
  FILE *pipe = popen(command.c_str(), "r");
  if (!pipe) return {};
  CommandCaptureResult result;
  char buffer[512] = {};
  while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) result.output += buffer;
  result.exit_code = DecodeProcessStatus(pclose(pipe));
  return result;
}
#endif

std::string RunProcessCapture(const std::vector<std::string> &args) {
#if defined(_WIN32)
  if (args.empty()) return {};
  SECURITY_ATTRIBUTES security_attributes{};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.bInheritHandle = TRUE;
  HANDLE read_pipe = nullptr;
  HANDLE write_pipe = nullptr;
  if (!CreatePipe(&read_pipe, &write_pipe, &security_attributes, 0)) return {};
  SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOA startup_info{};
  startup_info.cb = sizeof(startup_info);
  startup_info.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
  startup_info.wShowWindow = SW_HIDE;
  startup_info.hStdOutput = write_pipe;
  startup_info.hStdError = write_pipe;
  PROCESS_INFORMATION process_info{};
  std::string command_line;
  for (const std::string &arg : args) {
    if (!command_line.empty()) command_line.push_back(' ');
    command_line += QuoteWinArg(arg);
  }
  const BOOL created = CreateProcessA(nullptr, command_line.data(), nullptr, nullptr, TRUE,
                                      CREATE_NO_WINDOW, nullptr, nullptr, &startup_info, &process_info);
  CloseHandle(write_pipe);
  if (!created) {
    CloseHandle(read_pipe);
    return {};
  }

  std::string output;
  char buffer[512] = {};
  DWORD bytes_read = 0;
  while (ReadFile(read_pipe, buffer, sizeof(buffer), &bytes_read, nullptr) && bytes_read > 0) {
    output.append(buffer, buffer + bytes_read);
  }
  WaitForSingleObject(process_info.hProcess, INFINITE);
  CloseHandle(process_info.hThread);
  CloseHandle(process_info.hProcess);
  CloseHandle(read_pipe);
  return output;
#else
  if (args.empty()) return {};
  std::ostringstream command;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i) command << ' ';
    command << EscapeForPosix(args[i]);
  }
  return RunCommandCapture(command.str());
#endif
}

#if !defined(_WIN32)
std::string PosixCleanEnvCommandPrefix() {
  return "env -u LD_LIBRARY_PATH ";
}
#endif

int RunCommand(const std::string &command) {
#if defined(_WIN32)
  STARTUPINFOA startup_info{};
  startup_info.cb = sizeof(startup_info);
  startup_info.dwFlags = STARTF_USESHOWWINDOW;
  startup_info.wShowWindow = SW_HIDE;
  PROCESS_INFORMATION process_info{};
  std::string command_line = "cmd.exe /C " + command;
  if (!CreateProcessA(nullptr, command_line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                      &startup_info, &process_info)) {
    return -1;
  }
  WaitForSingleObject(process_info.hProcess, INFINITE);
  DWORD exit_code = 1;
  GetExitCodeProcess(process_info.hProcess, &exit_code);
  CloseHandle(process_info.hThread);
  CloseHandle(process_info.hProcess);
  return static_cast<int>(exit_code);
#else
  return std::system(command.c_str());
#endif
}

#ifdef HAVE_LIBCURL
struct CurlGlobalInit {
  CurlGlobalInit() { curl_global_init(CURL_GLOBAL_DEFAULT); }
  ~CurlGlobalInit() { curl_global_cleanup(); }
};

bool EnvEnabled(const char *name) {
  const char *value = std::getenv(name);
  return value && *value && std::string(value) != "0";
}

bool UseLibcurlTransport() {
  if (EnvEnabled("ROCREADER_DISABLE_LIBCURL")) return false;
  if (const char *value = std::getenv("ROCREADER_ONLINE_TRANSPORT"); value && *value) {
    std::string mode = value;
    std::transform(mode.begin(), mode.end(), mode.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (mode == "command" || mode == "cmd" || mode == "curl" || mode == "wget") return false;
    if (mode == "libcurl") return true;
  }
  return true;
}

void EnsureCurlGlobalInit() {
  static CurlGlobalInit init;
  static bool logged = false;
  if (!logged) {
    logged = true;
    const curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
    runtime_log::Line(std::string("online: libcurl transport enabled version=") +
                      (info && info->version ? info->version : "unknown") +
                      " ssl=" + (info && info->ssl_version ? info->ssl_version : "none"));
  }
  (void)init;
}

size_t CurlWriteString(char *ptr, size_t size, size_t nmemb, void *userdata) {
  if (!userdata) return 0;
  std::string *out = static_cast<std::string *>(userdata);
  out->append(ptr, size * nmemb);
  return size * nmemb;
}

size_t CurlWriteFile(char *ptr, size_t size, size_t nmemb, void *userdata) {
  if (!userdata) return 0;
  FILE *file = static_cast<FILE *>(userdata);
  return std::fwrite(ptr, size, nmemb, file) * size;
}

void ConfigureCurlCommon(CURL *curl, const std::string &url, const std::string &referer) {
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 8L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "ROCreader-OPDS");
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  if (!referer.empty()) curl_easy_setopt(curl, CURLOPT_REFERER, referer.c_str());
}

std::string HttpGetTextViaLibcurl(const std::string &url, const std::string &referer) {
  if (url.empty()) return {};
  EnsureCurlGlobalInit();
  runtime_log::Line("online: libcurl GET begin url=" + url);
  CURL *curl = curl_easy_init();
  if (!curl) {
    runtime_log::Line("online: libcurl GET init failed url=" + url);
    return {};
  }
  std::string body;
  ConfigureCurlCommon(curl, url, referer);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteString);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  const CURLcode code = curl_easy_perform(curl);
  runtime_log::Line("online: libcurl GET performed code=" + std::to_string(static_cast<int>(code)) +
                    " url=" + url + " bytes=" + std::to_string(body.size()));
  long response_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
  if (code != CURLE_OK) {
    runtime_log::Line("online: libcurl GET failed code=" + std::to_string(static_cast<int>(code)) +
                      " http=" + std::to_string(response_code) +
                      " url=" + url + " err=" + curl_easy_strerror(code));
    body.clear();
  }
  curl_easy_cleanup(curl);
  return body;
}

std::string HttpPostJsonTextViaLibcurl(const std::string &url, const std::string &json,
                                       const std::string &origin, const std::string &referer) {
  if (url.empty()) return {};
  EnsureCurlGlobalInit();
  runtime_log::Line("online: libcurl POST begin url=" + url + " bytes=" + std::to_string(json.size()));
  CURL *curl = curl_easy_init();
  if (!curl) {
    runtime_log::Line("online: libcurl POST init failed url=" + url);
    return {};
  }
  std::string body;
  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  if (!origin.empty()) headers = curl_slist_append(headers, ("Origin: " + origin).c_str());
  ConfigureCurlCommon(curl, url, referer);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json.size()));
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteString);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  const CURLcode code = curl_easy_perform(curl);
  runtime_log::Line("online: libcurl POST performed code=" + std::to_string(static_cast<int>(code)) +
                    " url=" + url + " bytes=" + std::to_string(body.size()));
  long response_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
  if (code != CURLE_OK) {
    runtime_log::Line("online: libcurl POST failed code=" + std::to_string(static_cast<int>(code)) +
                      " http=" + std::to_string(response_code) +
                      " url=" + url + " err=" + curl_easy_strerror(code));
    body.clear();
  }
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return body;
}

bool DownloadFileViaLibcurl(const std::string &url, const std::filesystem::path &output_path,
                            const std::string &referer) {
  if (url.empty() || output_path.empty()) return false;
  std::error_code ec;
  std::filesystem::create_directories(output_path.parent_path(), ec);
  FILE *file = std::fopen(output_path.string().c_str(), "wb");
  if (!file) return false;
  EnsureCurlGlobalInit();
  runtime_log::Line("online: libcurl download begin url=" + url + " output=" + output_path.string());
  CURL *curl = curl_easy_init();
  if (!curl) {
    runtime_log::Line("online: libcurl download init failed url=" + url);
    std::fclose(file);
    std::filesystem::remove(output_path, ec);
    return false;
  }
  ConfigureCurlCommon(curl, url, referer);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteFile);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
  const CURLcode code = curl_easy_perform(curl);
  runtime_log::Line("online: libcurl download performed code=" + std::to_string(static_cast<int>(code)) +
                    " url=" + url + " output=" + output_path.string());
  long response_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
  curl_easy_cleanup(curl);
  std::fclose(file);
  if (code == CURLE_OK) return true;
  runtime_log::Line("online: libcurl download failed code=" + std::to_string(static_cast<int>(code)) +
                    " http=" + std::to_string(response_code) +
                    " url=" + url + " output=" + output_path.string() +
                    " err=" + curl_easy_strerror(code));
  std::filesystem::remove(output_path, ec);
  return false;
}
#endif

bool StartsWith(const std::string &text, const std::string &prefix) {
  return text.rfind(prefix, 0) == 0;
}

std::string ResolveUrl(const std::string &base_url, const std::string &href) {
  if (href.empty()) return {};
  std::string lower = href;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
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

std::string OriginFromUrl(const std::string &url) {
  const size_t scheme = url.find("://");
  if (scheme == std::string::npos) return {};
  const size_t authority_begin = scheme + 3;
  const size_t path_begin = url.find('/', authority_begin);
  return path_begin == std::string::npos ? url : url.substr(0, path_begin);
}

std::string JsonEscape(const std::string &text) {
  std::string out;
  out.reserve(text.size() + 8);
  for (char ch : text) {
    if (ch == '\\') out += "\\\\";
    else if (ch == '"') out += "\\\"";
    else if (ch == '\n') out += "\\n";
    else if (ch == '\r') out += "\\r";
    else if (ch == '\t') out += "\\t";
    else out.push_back(ch);
  }
  return out;
}

std::string SafeFilename(std::string name) {
  name = Trim(name);
  if (name.empty()) name = "online_book";
  std::string ascii;
  ascii.reserve(name.size());
  for (char &ch : name) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (uch >= 128 || ch == '\\' || ch == '/' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' ||
        ch == '>' || ch == '|' || uch < 32) {
      ascii.push_back('_');
    } else {
      ascii.push_back(ch);
    }
  }
  while (!ascii.empty() && (ascii.back() == ' ' || ascii.back() == '.')) ascii.pop_back();
  while (ascii.find("__") != std::string::npos) ascii.replace(ascii.find("__"), 2, "_");
  if (ascii.empty() || ascii == "_") ascii = "online_book";
  if (ascii.size() > 80) ascii.resize(80);
  return ascii;
}

std::filesystem::path ManualWebHelperPath() {
  std::filesystem::path helper = std::filesystem::current_path() / "Windows" / "manual_web_fetch.py";
  std::error_code ec;
  if (!std::filesystem::exists(helper, ec) || ec) helper = std::filesystem::current_path() / "manual_web_fetch.py";
  if (!std::filesystem::exists(helper, ec) || ec) return {};
  return helper;
}

bool ManualWebExternalTransportEnabled() {
  const char *value = std::getenv("ROCREADER_MANUAL_WEB_TRANSPORT");
  if (!value || !*value) value = std::getenv("ROCREADER_EXPERIMENTAL_WN04_TRANSPORT");
  return value && *value && std::string(value) != "0";
}

bool ManualWebCatalogOnlyEnabled() {
  const char *value = std::getenv("ROCREADER_MANUAL_WEB_CATALOG_ONLY");
  if (!value || !*value) value = std::getenv("ROCREADER_WN04_CATALOG_ONLY");
  return value && *value && std::string(value) != "0";
}

bool IsWn04Url(const std::string &url) {
  return url.find("wn04.cfd") != std::string::npos || url.find("wnacg") != std::string::npos ||
         url.find("wcdn.date") != std::string::npos;
}

std::filesystem::path ManualWebExternalHelperPath() {
  if (!ManualWebExternalTransportEnabled()) return {};
  if (const char *value = std::getenv("ROCREADER_MANUAL_WEB_FETCH"); value && *value) {
    std::filesystem::path helper = value;
    std::error_code ec;
    if (std::filesystem::exists(helper, ec) && !ec) return helper;
    runtime_log::Line("online: manual web helper missing env path=" + helper.string());
    return {};
  }
  if (const char *value = std::getenv("ROCREADER_WN04_FETCH"); value && *value) {
    std::filesystem::path helper = value;
    std::error_code ec;
    if (std::filesystem::exists(helper, ec) && !ec) return helper;
    runtime_log::Line("online: manual web helper missing legacy env path=" + helper.string());
    return {};
  }
  const std::vector<std::filesystem::path> candidates = {
      std::filesystem::current_path() / "bin" / "wn04_fetch",
      std::filesystem::current_path() / "wn04_fetch",
  };
  std::error_code ec;
  for (const auto &candidate : candidates) {
    if (std::filesystem::exists(candidate, ec) && !ec) return candidate;
  }
  return {};
}

std::string ManualWebFetchViaExternalHelper(const std::string &url, const std::string &referer) {
#if !defined(_WIN32)
  const std::filesystem::path helper = ManualWebExternalHelperPath();
  if (helper.empty()) return {};
  runtime_log::Line("online: manual web helper fetch path=" + helper.string() + " url=" + url);
  CommandCaptureResult result = RunCommandCaptureWithStatus(EscapeForPosix(helper.string()) + " fetch " +
                                                            EscapeForPosix(url) +
                                                            (referer.empty() ? "" : " " + EscapeForPosix(referer)) +
                                                            " 2>&1");
  if (result.exit_code == 0 && !result.output.empty()) return result.output;
  runtime_log::Line("online: manual web helper fetch failed exit=" + std::to_string(result.exit_code) +
                    " url=" + url + " output=" + CompactLogSnippet(result.output));
#else
  (void)url;
  (void)referer;
#endif
  return {};
}

std::string ManualWebResolveViaExternalHelper(const std::string &detail_url, const std::string &title,
                                              const std::string &source_url) {
#if !defined(_WIN32)
  const std::filesystem::path helper = ManualWebExternalHelperPath();
  if (helper.empty()) return {};
  runtime_log::Line("online: manual web helper resolve path=" + helper.string() +
                    " detail_url=" + detail_url + " title=" + title);
  CommandCaptureResult result = RunCommandCaptureWithStatus(EscapeForPosix(helper.string()) + " resolve " +
                                                            EscapeForPosix(detail_url) + " " +
                                                            EscapeForPosix(title) + " " +
                                                            EscapeForPosix(source_url) + " 2>&1");
  const std::string url = ExtractJsonStringValue(result.output, "url");
  if (result.exit_code == 0 && !url.empty()) return url;
  runtime_log::Line("online: manual web helper resolve failed exit=" + std::to_string(result.exit_code) +
                    " detail_url=" + detail_url + " output=" + CompactLogSnippet(result.output));
#else
  (void)detail_url;
  (void)title;
  (void)source_url;
#endif
  return {};
}

bool ManualWebDownloadViaExternalHelper(const std::string &url, const std::filesystem::path &output_path,
                                        const std::string &referer) {
#if !defined(_WIN32)
  const std::filesystem::path helper = ManualWebExternalHelperPath();
  if (helper.empty()) return false;
  std::error_code ec;
  std::filesystem::create_directories(output_path.parent_path(), ec);
  runtime_log::Line("online: manual web helper download path=" + helper.string() +
                    " url=" + url + " output=" + output_path.string());
  const int exit_code = RunCommand(EscapeForPosix(helper.string()) + " download " + EscapeForPosix(url) + " " +
                                   EscapeForPosix(output_path.string()) +
                                   (referer.empty() ? "" : " " + EscapeForPosix(referer)));
  if (exit_code == 0 && std::filesystem::exists(output_path, ec) && !ec) return true;
  runtime_log::Line("online: manual web helper download failed exit=" + std::to_string(exit_code) +
                    " url=" + url + " output=" + output_path.string());
  std::filesystem::remove(output_path, ec);
#else
  (void)url;
  (void)output_path;
  (void)referer;
#endif
  return false;
}

std::string ManualWebFetchViaPython(const std::string &url, const std::string &referer) {
#if defined(_WIN32)
  const std::filesystem::path helper = ManualWebHelperPath();
  if (helper.empty()) return {};
  return RunProcessCapture({"py", "-3", helper.string(), "fetch", url, referer});
#else
  (void)url;
  (void)referer;
  return {};
#endif
}

std::string ManualWebResolveViaPython(const std::string &detail_url, const std::string &title,
                                      const std::string &source_url) {
#if defined(_WIN32)
  const std::filesystem::path helper = ManualWebHelperPath();
  if (helper.empty()) return {};
  runtime_log::Line("online: manual web helper resolve invoke helper=" + helper.string() +
                    " detail_url=" + detail_url + " title=" + title + " source_url=" + source_url);
  const std::string json = RunProcessCapture({"py", "-3", helper.string(), "resolve", detail_url, title, source_url});
  const std::string url = ExtractJsonStringValue(json, "url");
  if (url.empty()) {
    const std::string error = ExtractJsonStringValue(json, "error");
    const std::string detail = ExtractJsonStringValue(json, "detail");
    if (!error.empty() || !detail.empty()) {
      runtime_log::Line("online: manual web helper resolve failed error=" + error +
                        " detail=" + CompactLogSnippet(detail.empty() ? json : detail) +
                        " detail_url=" + detail_url);
    }
  }
  return url;
#else
  (void)detail_url;
  (void)title;
  (void)source_url;
  return {};
#endif
}

bool ManualWebDownloadViaPython(const std::string &url, const std::filesystem::path &output_path,
                                const std::string &referer) {
#if defined(_WIN32)
  const std::filesystem::path helper = ManualWebHelperPath();
  if (helper.empty()) return false;
  std::error_code ec;
  std::filesystem::create_directories(output_path.parent_path(), ec);
  const std::string output =
      RunProcessCapture({"py", "-3", helper.string(), "download", url, output_path.string(), referer});
  std::error_code exists_ec;
  if (std::filesystem::exists(output_path, exists_ec) && !exists_ec) return true;
  runtime_log::Line("online: manual web helper download failed url=" + url +
                    " output=" + output_path.string() +
                    " detail=" + CompactLogSnippet(output));
  return false;
#else
  (void)url;
  (void)output_path;
  (void)referer;
  return false;
#endif
}
}  // namespace

std::string CompactLogSnippet(const std::string &text, size_t max_len) {
  std::string trimmed = Trim(text);
  if (trimmed.size() <= max_len) return trimmed;
  return trimmed.substr(0, max_len) + "...";
}

std::string RunHiddenProcessCapture(const std::vector<std::string> &args) {
  return RunProcessCapture(args);
}

std::string HttpGetText(const std::string &url, const std::string &referer) {
#ifdef HAVE_LIBCURL
  if (UseLibcurlTransport()) {
    if (std::string output = HttpGetTextViaLibcurl(url, referer); !output.empty()) return output;
  } else {
    runtime_log::Line("online: libcurl GET skipped by transport mode url=" + url);
  }
#endif
#if defined(_WIN32)
  const std::string curl_referer = referer.empty() ? std::string() : " -e " + EscapeForCmdDoubleQuote(referer);
  std::string output = RunCommandCapture("curl.exe -LfsS -A " + EscapeForCmdDoubleQuote("ROCreader-OPDS") +
                                         curl_referer + " " + EscapeForCmdDoubleQuote(url) + " 2>NUL");
  if (!output.empty()) return output;
  const std::string escaped_url = EscapeForPowerShell(url);
  const std::string referer_header = referer.empty()
                                         ? std::string()
                                         : "'Referer'='" + EscapeForPowerShell(referer) + "'; ";
  return RunCommandCapture(
      "powershell -NoProfile -Command \"[Net.ServicePointManager]::SecurityProtocol = "
      "[Net.SecurityProtocolType]::Tls12; $ProgressPreference='SilentlyContinue'; "
      "(Invoke-WebRequest -UseBasicParsing -Headers @{ " + referer_header +
      "'User-Agent'='ROCreader-OPDS' } -Uri '"
      + escaped_url + "').Content\" 2>NUL");
#else
  const std::string quoted_url = EscapeForPosix(url);
  const std::string header = EscapeForPosix("User-Agent: ROCreader-OPDS");
  const std::string referer_arg = referer.empty() ? std::string() : " -e " + EscapeForPosix(referer);
  const std::string prefix = PosixCleanEnvCommandPrefix();
  CommandCaptureResult curl_result =
      RunCommandCaptureWithStatus(prefix + "curl -LfsS --connect-timeout 20 --max-time 60 -H " + header +
                                  referer_arg + " " + quoted_url + " 2>&1");
  if (!curl_result.output.empty() && curl_result.exit_code == 0) return curl_result.output;
  runtime_log::Line("online: command curl GET failed exit=" + std::to_string(curl_result.exit_code) +
                    " url=" + url + " output=" + CompactLogSnippet(curl_result.output));
  CommandCaptureResult wget_result =
      RunCommandCaptureWithStatus(prefix + "wget -qO- --timeout=60 --user-agent=" +
                                  EscapeForPosix("ROCreader-OPDS") +
                                  (referer.empty() ? "" : " --referer=" + EscapeForPosix(referer)) +
                                  " " + quoted_url + " 2>&1");
  if (!wget_result.output.empty() && wget_result.exit_code == 0) return wget_result.output;
  runtime_log::Line("online: command wget GET failed exit=" + std::to_string(wget_result.exit_code) +
                    " url=" + url + " output=" + CompactLogSnippet(wget_result.output));
  runtime_log::Line("online: command GET failed url=" + url);
  return {};
#endif
}

std::string ManualWebFetch(const std::string &url, const std::string &referer) {
  if (std::string body = ManualWebFetchViaExternalHelper(url, referer); !body.empty()) return body;
  if (std::string body = ManualWebFetchViaPython(url, referer); !body.empty()) return body;
  return HttpGetText(url, referer);
}

std::string ManualWebResolveDownload(const std::string &detail_url, const std::string &title,
                                     const std::string &source_url) {
  if (ManualWebExternalTransportEnabled() && ManualWebCatalogOnlyEnabled() &&
      (IsWn04Url(detail_url) || IsWn04Url(source_url))) {
    runtime_log::Line("online: manual web catalog-only skip download resolve title=" + title +
                      " detail_url=" + detail_url);
    return {};
  }
  if (std::string real_url = ManualWebResolveViaExternalHelper(detail_url, title, source_url);
      !real_url.empty()) {
    return real_url;
  }
  if (std::string real_url = ManualWebResolveViaPython(detail_url, title, source_url); !real_url.empty()) {
    return real_url;
  }

  const std::string detail_html = HttpGetText(detail_url, source_url);
  if (detail_html.empty()) {
    runtime_log::Line("online: manual web resolve detail fetch failed title=" + title +
                      " detail_url=" + detail_url);
    return {};
  }
  std::smatch match;
  if (!std::regex_search(detail_html, match,
                         std::regex(R"(<a[^>]+href\s*=\s*(['"])([^'"]*/download-index-aid-[^'"]*)\1)",
                                    std::regex::icase))) {
    runtime_log::Line("online: manual web resolve landing link missing title=" + title +
                      " detail_url=" + detail_url);
    return {};
  }
  const std::string landing_url = ResolveUrl(detail_url, match[2].str());
  const std::string landing_html = HttpGetText(landing_url, detail_url);
  if (landing_html.empty()) {
    runtime_log::Line("online: manual web resolve landing fetch failed title=" + title +
                      " landing_url=" + landing_url + " detail_url=" + detail_url);
    return {};
  }
  if (!std::regex_search(landing_html, match, std::regex(R"(['"]((?:down/)[^'"]+\.zip)['"])", std::regex::icase))) {
    runtime_log::Line("online: manual web resolve download key missing title=" + title +
                      " landing_url=" + landing_url);
    return {};
  }
  const std::string key = match[1].str();
  const std::string api = "https://d1.wcdn.date/api/generate-link";
  const std::string filename = SafeFilename(title) + ".zip";
  const std::string body = "{\"file_key\":\"" + JsonEscape(key) + "\",\"file_name\":\"" + JsonEscape(filename) + "\"}";
  const std::string origin = OriginFromUrl(source_url).empty() ? OriginFromUrl(detail_url) : OriginFromUrl(source_url);
  const std::string json = HttpPostJsonText(api, body, origin, landing_url);
  const std::string real_url = ExtractJsonStringValue(json, "url");
  if (real_url.empty()) {
    runtime_log::Line("online: manual web resolve generate-link failed title=" + title +
                      " key=" + key + " landing_url=" + landing_url +
                      " response=" + CompactLogSnippet(json));
  }
  return real_url;
}

bool ManualWebDownload(const std::string &url, const std::filesystem::path &output_path,
                       const std::string &referer) {
  if (ManualWebDownloadViaExternalHelper(url, output_path, referer)) return true;
  if (ManualWebDownloadViaPython(url, output_path, referer)) return true;
  return DownloadFile(url, output_path, referer);
}

std::string HttpPostJsonText(const std::string &url, const std::string &json,
                             const std::string &origin, const std::string &referer) {
#ifdef HAVE_LIBCURL
  if (UseLibcurlTransport()) {
    if (std::string output = HttpPostJsonTextViaLibcurl(url, json, origin, referer); !output.empty()) return output;
  } else {
    runtime_log::Line("online: libcurl POST skipped by transport mode url=" + url);
  }
#endif
  const std::string quoted_url = EscapeForPosix(url);
  const std::string quoted_json = EscapeForPosix(json);
  const std::string header_ua = EscapeForPosix("User-Agent: ROCreader-OPDS");
  const std::string header_ct = EscapeForPosix("Content-Type: application/json");
  const std::string origin_arg = origin.empty() ? std::string() : " -H " + EscapeForPosix("Origin: " + origin);
  const std::string referer_arg = referer.empty() ? std::string() : " -H " + EscapeForPosix("Referer: " + referer);
#if defined(_WIN32)
  std::string output = RunCommandCapture("curl.exe -LfsS -X POST -A " + EscapeForCmdDoubleQuote("ROCreader-OPDS") +
                                         " -H " + EscapeForCmdDoubleQuote("Content-Type: application/json") +
                                         " -H " + EscapeForCmdDoubleQuote("Origin: " + origin) +
                                         " -e " + EscapeForCmdDoubleQuote(referer) +
                                         " --data " + EscapeForCmdDoubleQuote(json) +
                                         " " + EscapeForCmdDoubleQuote(url) + " 2>NUL");
  if (!output.empty()) return output;
  const std::string escaped_url = EscapeForPowerShell(url);
  const std::string escaped_json = EscapeForPowerShell(json);
  const std::string escaped_origin = EscapeForPowerShell(origin);
  const std::string escaped_referer = EscapeForPowerShell(referer);
  return RunCommandCapture(
      "powershell -NoProfile -Command \"[Net.ServicePointManager]::SecurityProtocol = "
      "[Net.SecurityProtocolType]::Tls12; $ProgressPreference='SilentlyContinue'; "
      "(Invoke-WebRequest -UseBasicParsing -Method Post -ContentType 'application/json' "
      "-Headers @{ 'User-Agent'='ROCreader-OPDS'; 'Origin'='" + escaped_origin +
      "'; 'Referer'='" + escaped_referer + "' } -Uri '" + escaped_url +
      "' -Body '" + escaped_json + "').Content\" 2>NUL");
#else
  const std::string prefix = PosixCleanEnvCommandPrefix();
  CommandCaptureResult curl_result =
      RunCommandCaptureWithStatus(prefix + "curl -LfsS --connect-timeout 20 --max-time 60 -X POST -H " +
                                  header_ua + " -H " + header_ct + origin_arg + referer_arg +
                                  " --data " + quoted_json + " " + quoted_url + " 2>&1");
  if (curl_result.exit_code == 0 && !curl_result.output.empty()) return curl_result.output;
  runtime_log::Line("online: command POST failed exit=" + std::to_string(curl_result.exit_code) +
                    " url=" + url + " output=" + CompactLogSnippet(curl_result.output));
  return {};
#endif
}

bool DownloadFile(const std::string &url, const std::filesystem::path &output_path, const std::string &referer) {
  if (url.empty() || output_path.empty()) return false;
  std::error_code ec;
  std::filesystem::create_directories(output_path.parent_path(), ec);
#ifdef HAVE_LIBCURL
  if (UseLibcurlTransport()) {
    if (DownloadFileViaLibcurl(url, output_path, referer)) return true;
  } else {
    runtime_log::Line("online: libcurl download skipped by transport mode url=" + url +
                      " output=" + output_path.string());
  }
#endif
#if defined(_WIN32)
  const std::string curl_referer = referer.empty() ? std::string() : " -e " + EscapeForCmdDoubleQuote(referer);
  const int curl_exit = RunCommand("curl.exe -L --fail --silent --show-error -A " +
                                   EscapeForCmdDoubleQuote("ROCreader-OPDS") + curl_referer + " " +
                                   EscapeForCmdDoubleQuote(url) + " -o " +
                                   EscapeForCmdDoubleQuote(output_path.string()));
  if (curl_exit == 0) return true;
  runtime_log::Line("online: download curl failed exit=" + std::to_string(curl_exit) +
                    " url=" + url + " output=" + output_path.string());
  const std::string escaped_url = EscapeForPowerShell(url);
  const std::string escaped_path = EscapeForPowerShell(output_path.string());
  const std::string referer_header = referer.empty()
                                         ? std::string()
                                         : "'Referer'='" + EscapeForPowerShell(referer) + "'; ";
  const int ps_exit = RunCommand(
             "powershell -NoProfile -Command \"[Net.ServicePointManager]::SecurityProtocol = "
             "[Net.SecurityProtocolType]::Tls12; $ProgressPreference='SilentlyContinue'; "
             "Invoke-WebRequest -UseBasicParsing -Headers @{ " + referer_header +
             "'User-Agent'='ROCreader-OPDS' } -Uri '"
             + escaped_url + "' -OutFile '" + escaped_path + "'\" 2>NUL");
  if (ps_exit == 0) return true;
  runtime_log::Line("online: download powershell failed exit=" + std::to_string(ps_exit) +
                    " url=" + url + " output=" + output_path.string());
  return false;
#else
  const std::string quoted_url = EscapeForPosix(url);
  const std::string quoted_output = EscapeForPosix(output_path.string());
  const std::string header = EscapeForPosix("User-Agent: ROCreader-OPDS");
  const std::string referer_arg = referer.empty() ? std::string() : " -H " + EscapeForPosix("Referer: " + referer);
  const std::string prefix = PosixCleanEnvCommandPrefix();
  const int curl_exit = RunCommand(prefix + "curl -L --fail --silent --show-error -H " + header + referer_arg + " -o " +
                                   quoted_output + " " + quoted_url);
  if (curl_exit == 0) return true;
  runtime_log::Line("online: download curl failed exit=" + std::to_string(curl_exit) +
                    " url=" + url + " output=" + output_path.string());
  const std::string referer_wget = referer.empty() ? std::string() : " --referer=" + EscapeForPosix(referer);
  const int wget_exit = RunCommand(prefix + "wget -q --user-agent=" + EscapeForPosix("ROCreader-OPDS") + referer_wget +
                                   " -O " + quoted_output + " " + quoted_url);
  if (wget_exit != 0) {
    runtime_log::Line("online: download wget failed exit=" + std::to_string(wget_exit) +
                      " url=" + url + " output=" + output_path.string());
  }
  return wget_exit == 0;
#endif
}

std::string ExtractJsonStringValue(const std::string &json, const std::string &key) {
  const std::string needle = '"' + key + '"';
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
      return out;
    } else {
      out.push_back(c);
    }
  }
  return {};
}
