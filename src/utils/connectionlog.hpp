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

#ifndef NUNCHUK_CONNECTIONLOG_HPP
#define NUNCHUK_CONNECTIONLOG_HPP

#include <nunchuk.h>

#include <string>

namespace nunchuk {

const char* ConnectionLogBool(bool value);
const char* ConnectionStatusName(ConnectionStatus status);
std::string DefaultConnectionLogFilePath(const std::string& storage_path);
void SetConnectionLogFilePath(const std::string& path);
void ConnectionDebugLog(const char* component, const std::string& message);

}  // namespace nunchuk

#endif  // NUNCHUK_CONNECTIONLOG_HPP
