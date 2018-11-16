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
#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdexcept>

#include <unistd.h>
#include <sys/syslog.h>
#include <sys/syscall.h>
#include <sys/time.h>

namespace
{
  struct LevelMapping
  {
    char const* s;
    logLevel l;
  };

  LevelMapping mappings[] = 
  {
    { "CRIT",   logLevel_Critical },
    { "ERROR",  logLevel_Error },
    { "WARN",   logLevel_Warning },
    { "INFO",   logLevel_Info },
    { "DEBUG",  logLevel_Debug }
  };

  int const kNumMappings = sizeof(mappings) / sizeof(LevelMapping);
  char const* kUnknownMapping = "UNKNOWN";

  int toSyslogPriority(logLevel level)
  {
    int p = LOG_EMERG;
    switch (level)
    {
      case logLevel_Critical:
        p = LOG_CRIT;
        break;
      case logLevel_Error:
        p = LOG_ERR;
        break;
      case logLevel_Warning:
        p = LOG_WARNING;
        break;
      case logLevel_Info:
        p = LOG_INFO;
        break;
      case logLevel_Debug:
        p = LOG_DEBUG;
        break;
    }
    return LOG_MAKEPRI(LOG_FAC(LOG_LOCAL4), LOG_PRI(p));
  }
}

void 
Logger::log(logLevel level, char const* /*file*/, int /*line*/, char const* format, ...)
{

  va_list args;
  va_start(args, format);

  if (m_dest == logDest_Syslog)
  {
    int priority = toSyslogPriority(level);
    vsyslog(priority, format, args);
  }
  else
  {
    timeval tv;
    gettimeofday(&tv, 0);
    printf("%ld.%05ld (%5s) thr-%ld [%s] -- ", tv.tv_sec, tv.tv_usec, levelToString(level), syscall(__NR_gettid), "xconfigd");
    vprintf(format, args);
    printf("\n");
  }

  va_end(args);
}

char const*
Logger::levelToString(logLevel level)
{
  for (int i = 0; i < kNumMappings; ++i)
  {
    if (mappings[i].l == level)
      return mappings[i].s;
  }
  return kUnknownMapping;
}

logLevel
Logger::stringToLevel(char const* level)
{
  if (level)
  {
    for (int i = 0; i < kNumMappings; ++i)
    {
      if (strcasecmp(mappings[i].s, level) == 0)
        return mappings[i].l;
    }
  }
  throw std::runtime_error("invalid logging level " + std::string(level));
}

Logger&
Logger::getLogger()
{
  static Logger logger;
  return logger;
}

Logger::Logger()
  : m_level(logLevel_Info)
  , m_dest(logDest_Stdout)
{

}

void Logger::setLevel(logLevel level)
{
  m_level = level;
}

void Logger::setDestination(logDest dest)
{
  m_dest = dest;
}
