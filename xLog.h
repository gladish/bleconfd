/*
 * Copyright [2017] [Comcast, Corp.]
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <stdio.h>

enum logLevel
{
  logLevel_Critical,
  logLevel_Error,
  logLevel_Warning,
  logLevel_Info,
  logLevel_Debug
};

enum logDest
{
  logDest_Stdout,
  logDest_Syslog
};

class xLog
{
public:
  void log(logLevel level, char const* file, int line, char const* msg, ...)
    __attribute__ ((format (printf, 5, 6)));

  inline bool isLevelEnabled(logLevel level)
    { return level <= m_level; }

  void setLevel(logLevel level);
  void setDestination(logDest dest);

  static char const* levelToString(logLevel level);
  static logLevel stringToLevel(char const* level);

  static xLog& getLogger();

private:
  xLog();

private:
  logLevel  m_level;
  logDest   m_dest;
};

#define XLOG(LEVEL, FORMAT, ...) \
    do { if (xLog::getLogger().isLevelEnabled(LEVEL)) { \
        xLog::getLogger().log(LEVEL, "", __LINE__ - 2, FORMAT, ##__VA_ARGS__); \
    } } while (0)

#define XLOG_DEBUG(FORMAT, ...) XLOG(logLevel_Debug, FORMAT, ##__VA_ARGS__)
#define XLOG_INFO(FORMAT, ...) XLOG(logLevel_Info, FORMAT, ##__VA_ARGS__)
#define XLOG_WARN(FORMAT, ...) XLOG(logLevel_Warning, FORMAT, ##__VA_ARGS__)
#define XLOG_ERROR(FORMAT, ...) XLOG(logLevel_Error, FORMAT, ##__VA_ARGS__)
#define XLOG_CRITICAL(FORMAT, ...) XLOG(logLevel_Critical, FORMAT, ##__VA_ARGS__)

#define XLOG_JSON(LEVEL, JSON) \
  do { if (xLog::getLogger().isLevelEnabled(LEVEL)) {\
    char* s = cJSON_Print(JSON); \
    xLog::getLogger().log(LEVEL, "", __LINE__ -2, "%s", s); \
    free(s); \
  } } while (0)

#endif
