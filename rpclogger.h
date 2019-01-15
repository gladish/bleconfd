//
// Copyright [2018] [Comcast, Corp]
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <stdio.h>

enum class RpcLogLevel
{
  Critical,
  Error,
  Warning,
  Info,
  Debug
};

enum class RpcLogDestination
{
  Stdout,
  Syslog
};

class RpcLogger
{
public:
  void log(RpcLogLevel level, char const* file, int line, char const* msg, ...)
    __attribute__ ((format (printf, 5, 6)));

  inline bool isLevelEnabled(RpcLogLevel level)
    { return level <= m_level; }

  void setLevel(RpcLogLevel level);
  void setDestination(RpcLogDestination dest);

  static char const* levelToString(RpcLogLevel level);
  static RpcLogLevel stringToLevel(char const* level);

  static RpcLogger& logger();

private:
  RpcLogger();

private:
  RpcLogLevel       m_level;
  RpcLogDestination m_dest;
};

#define XLOG(LEVEL, FORMAT, ...) \
    do { if (RpcLogger::logger().isLevelEnabled(LEVEL)) { \
        RpcLogger::logger().log(LEVEL, "", __LINE__ - 2, FORMAT, ##__VA_ARGS__); \
    } } while (0)

#define XLOG_DEBUG(FORMAT, ...) XLOG(RpcLogLevel::Debug, FORMAT, ##__VA_ARGS__)
#define XLOG_INFO(FORMAT, ...) XLOG(RpcLogLevel::Info, FORMAT, ##__VA_ARGS__)
#define XLOG_WARN(FORMAT, ...) XLOG(RpcLogLevel::Warning, FORMAT, ##__VA_ARGS__)
#define XLOG_ERROR(FORMAT, ...) XLOG(RpcLogLevel::Error, FORMAT, ##__VA_ARGS__)
#define XLOG_CRITICAL(FORMAT, ...) XLOG(RpcLogLevel::Critical, FORMAT, ##__VA_ARGS__)
#define XLOG_FATAL(FORMAT, ...) XLOG(RpcLogLevel::Critical, FORMAT, ##__VA_ARGS__)

#define XLOG_JSON(LEVEL, JSON) \
  do { if (RpcLogger::logger().isLevelEnabled(LEVEL)) {\
    char* s = cJSON_Print(JSON); \
    RpcLogger::logger().log(LEVEL, "", __LINE__ -2, "%s", s); \
    free(s); \
  } } while (0)

#endif
