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
#include "rpclogger.h"

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
    RpcLogLevel l;
  };

  LevelMapping mappings[] = 
  {
    { "CRIT",   RpcLogLevel::Critical },
    { "ERROR",  RpcLogLevel::Error },
    { "WARN",   RpcLogLevel::Warning },
    { "INFO",   RpcLogLevel::Info },
    { "DEBUG",  RpcLogLevel::Debug }
  };

  int const kNumMappings = sizeof(mappings) / sizeof(LevelMapping);
  char const* kUnknownMapping = "UNKNOWN";

  int toSyslogPriority(RpcLogLevel level)
  {
    int p = LOG_EMERG;
    switch (level)
    {
      case RpcLogLevel::Critical:
        p = LOG_CRIT;
        break;
      case RpcLogLevel::Error:
        p = LOG_ERR;
        break;
      case RpcLogLevel::Warning:
        p = LOG_WARNING;
        break;
      case RpcLogLevel::Info:
        p = LOG_INFO;
        break;
      case RpcLogLevel::Debug:
        p = LOG_DEBUG;
        break;
    }
    return LOG_MAKEPRI(LOG_FAC(LOG_LOCAL4), LOG_PRI(p));
  }
}

void 
RpcLogger::log(RpcLogLevel level, char const* /*file*/, int /*line*/, char const* format, ...)
{

  va_list args;
  va_start(args, format);

  if (m_dest == RpcLogDestination::Syslog)
  {
    int priority = toSyslogPriority(level);
    vsyslog(priority, format, args);
  }
  else
  {
    timeval tv;
    gettimeofday(&tv, 0);
    printf("%ld.%06ld (%5s) thr-%ld [%s] -- ", tv.tv_sec, tv.tv_usec, levelToString(level), syscall(__NR_gettid), "xconfigd");
    vprintf(format, args);
    printf("\n");
  }

  va_end(args);
}

char const*
RpcLogger::levelToString(RpcLogLevel level)
{
  for (int i = 0; i < kNumMappings; ++i)
  {
    if (mappings[i].l == level)
      return mappings[i].s;
  }
  return kUnknownMapping;
}

RpcLogLevel
RpcLogger::stringToLevel(char const* level)
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

RpcLogger&
RpcLogger::logger()
{
  static RpcLogger logger;
  return logger;
}

RpcLogger::RpcLogger()
  : m_level(RpcLogLevel::Info)
  , m_dest(RpcLogDestination::Stdout)
{

}

void RpcLogger::setLevel(RpcLogLevel level)
{
  m_level = level;
}

void RpcLogger::setDestination(RpcLogDestination dest)
{
  m_dest = dest;
}
