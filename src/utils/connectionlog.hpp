#ifndef NUNCHUK_UTILS_CONNECTIONLOG_HPP
#define NUNCHUK_UTILS_CONNECTIONLOG_HPP

#include <string>

namespace nunchuk {

const char* ConnectionLogBool(bool value);
void SetConnectionLogFilePath(const std::string& path);
std::string GetConnectionLogFilePath();
std::string DefaultConnectionLogFilePath(const std::string& storage_path);
void ConnectionDebugLog(const char* component, const std::string& message);

}  // namespace nunchuk

#endif  // NUNCHUK_UTILS_CONNECTIONLOG_HPP
