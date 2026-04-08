/*
 * This file is part of libnunchuk (https://github.com/nunchuk-io/libnunchuk).
 * Copyright (c) 2020 Enigmo.
 *
 * libnunchuk is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * libnunchuk is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libnunchuk. If not, see <http://www.gnu.org/licenses/>.
 */

#include <utils/connectionlog.hpp>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <system_error>

#include <utils/loguru.hpp>

namespace nunchuk {

namespace fs = std::filesystem;

namespace {

constexpr char kConnectionLogPrefix[] = "[connection][";
constexpr char kConnectionCallbackId[] = "libnunchuk.connection.log";

struct ConnectionLogSink {
  FILE* file;
};

std::mutex s_connection_log_mutex;
std::string s_connection_log_file_path;
bool s_connection_log_file_added = false;

bool IsConnectionLogMessage(const loguru::Message& message) {
  return message.message != nullptr &&
         std::strncmp(message.message, kConnectionLogPrefix,
                      sizeof(kConnectionLogPrefix) - 1) == 0;
}

void ConnectionFileLog(void* user_data, const loguru::Message& message) {
  if (!IsConnectionLogMessage(message)) return;

  auto* sink = static_cast<ConnectionLogSink*>(user_data);
  if (sink == nullptr || sink->file == nullptr) return;

  std::fprintf(sink->file, "%s%s%s%s\n", message.preamble,
               message.indentation, message.prefix, message.message);
  if (loguru::g_flush_interval_ms == 0) {
    std::fflush(sink->file);
  }
}

void ConnectionFileClose(void* user_data) {
  auto* sink = static_cast<ConnectionLogSink*>(user_data);
  if (sink == nullptr) return;
  if (sink->file != nullptr) {
    std::fclose(sink->file);
  }
  delete sink;
}

void ConnectionFileFlush(void* user_data) {
  auto* sink = static_cast<ConnectionLogSink*>(user_data);
  if (sink == nullptr || sink->file == nullptr) return;
  std::fflush(sink->file);
}

bool AddConnectionLogFileLocked(const std::string& path) {
  if (path.empty()) return false;

  std::error_code ec;
  auto parent = fs::path(path).parent_path();
  if (!parent.empty()) {
    fs::create_directories(parent, ec);
    if (ec) {
      LOG_F(ERROR, "Failed to create connection log directory '%s': %s",
            parent.string().c_str(), ec.message().c_str());
      return false;
    }
  }

#ifdef _WIN32
  FILE* file = nullptr;
  errno_t file_error = fopen_s(&file, path.c_str(), "a");
  if (file_error != 0 || file == nullptr) {
#else
  FILE* file = std::fopen(path.c_str(), "a");
  if (file == nullptr) {
#endif
    LOG_F(ERROR, "Failed to open connection log file '%s'", path.c_str());
    return false;
  }

  auto* sink = new ConnectionLogSink{file};
  loguru::add_callback(kConnectionCallbackId, ConnectionFileLog, sink,
                       loguru::Verbosity_INFO, ConnectionFileClose,
                       ConnectionFileFlush);
  return true;
}

}  // namespace

const char* ConnectionLogBool(bool value) { return value ? "true" : "false"; }

const char* ConnectionStatusName(ConnectionStatus status) {
  switch (status) {
    case ConnectionStatus::OFFLINE:
      return "OFFLINE";
    case ConnectionStatus::SYNCING:
      return "SYNCING";
    case ConnectionStatus::ONLINE:
      return "ONLINE";
  }
  return "UNKNOWN";
}

std::string DefaultConnectionLogFilePath(const std::string& storage_path) {
  if (storage_path.empty()) return {};
  return (fs::path(storage_path) / "logs" / "libnunchuk-connection.log")
      .string();
}

void SetConnectionLogFilePath(const std::string& path) {
  std::lock_guard<std::mutex> lock(s_connection_log_mutex);
  if (s_connection_log_file_path == path && s_connection_log_file_added) return;

  if (s_connection_log_file_added) {
    loguru::remove_callback(kConnectionCallbackId);
    s_connection_log_file_added = false;
  }

  s_connection_log_file_path = path;
  if (!s_connection_log_file_path.empty()) {
    s_connection_log_file_added =
        AddConnectionLogFileLocked(s_connection_log_file_path);
  }
}

void ConnectionDebugLog(const char* component, const std::string& message) {
  LOG_F(INFO, "[connection][%s] %s", component, message.c_str());
}

}  // namespace nunchuk
