#pragma once

#include <string>

namespace runtime_log {

void Init(const char *argv0 = nullptr);
void Line(const std::string &message);
void Line(const char *message);
std::string Path();

} // namespace runtime_log
