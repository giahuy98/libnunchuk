#include <utils/connectionlog.hpp>

#include <filesystem>
#include <mutex>
#include <utils/loguru.hpp>

namespace nunchuk {

namespace fs = std::filesystem;

namespace {

std::mutex s_connection_log_mutex;
std::string s_connection_log_file_path;
bool s_connection_log_file_added = false;

}  // namespace

const char* ConnectionLogBool(bool value) {
  return value ? "true" : "false";
}

void SetConnectionLogFilePath(const std::string& path) {
  std::lock_guard<std::mutex> lock(s_connection_log_mutex);
  if (s_connection_log_file_path == path) {
    return;
  }

  if (s_connection_log_file_added) {
    loguru::remove_callback(s_connection_log_file_path.c_str());
    s_connection_log_file_added = false;
  }

  s_connection_log_file_path = path;
  if (!s_connection_log_file_path.empty()) {
    s_connection_log_file_added = loguru::add_file(
        s_connection_log_file_path.c_str(), loguru::Append,
        loguru::Verbosity_INFO);
  }
}

std::string GetConnectionLogFilePath() {
  std::lock_guard<std::mutex> lock(s_connection_log_mutex);
  return s_connection_log_file_path;
}

std::string DefaultConnectionLogFilePath(const std::string& storage_path) {
  if (storage_path.empty()) {
    return {};
  }
  return (fs::path(storage_path) / "logs" / "libnunchuk-connection.log")
      .string();
}

void ConnectionDebugLog(const char* component, const std::string& message) {
  LOG_F(INFO, "[%s] %s", component, message.c_str());
}

}  // namespace nunchuk
