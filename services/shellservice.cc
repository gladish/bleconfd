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
#include "shellservice.h"
#include "../rpclogger.h"
#include "../jsonrpc.h"

#include <sstream>
#include <string.h>

JSONRPC_SERVICE_DEFINE(cmd, []{return new ShellService();});

namespace
{
  cJSON const* 
  findCommand(cJSON const* cmds, char const* method)
  {
    for (int i =0, n = cJSON_GetArraySize(cmds); i < n; ++i)
    {
      cJSON const* temp = cJSON_GetArrayItem(cmds, i);
      cJSON const* name = cJSON_GetObjectItem(temp, "name");
      if (name && strcmp(name->valuestring, method) == 0)
        return temp;
    }
    return nullptr;
  }

  cJSON*
  invokeShellCommand(cJSON const* config, cJSON const* args)
  {
    cJSON* res = nullptr;

    // Should have several options
    // 1. copy stdout true|false
    // 2. copy stderr true|false
    // ${arg:1} ${arg:2}
    std::string path = JsonRpc::getStringWithExpansion(config, "/exec", true,
      nullptr, args);

    XLOG_INFO("popen:%s", path.c_str());

    FILE* in = popen(path.c_str(), "r");
    if (in)
    {
      std::stringstream output;

      char buff[256];
      memset(buff, 0, sizeof(buff));
      while (fgets(buff, sizeof(buff), in))
      {
        output << buff;
        memset(buff, 0, sizeof(buff));
      }

      res = cJSON_CreateObject();
      cJSON_AddItemToObject(res, "return_code", cJSON_CreateNumber(0));
      cJSON_AddItemToObject(res, "stdout", cJSON_CreateString(output.str().c_str()));
      pclose(in);
    }
    else
    {
      int err = errno;
      res = JsonRpc::makeError(err, "failed to execute %s. %s",
        path.c_str(), strerror(errno));
    }

    return res;
  }
}

ShellService::ShellService()
  : BasicRpcService("cmd")
{
}

ShellService::~ShellService()
{
}

void
ShellService::init(cJSON const* conf, RpcNotificationFunction const& callback)
{
  BasicRpcService::init(conf, callback);
  registerMethod("exec", [this](cJSON const* req) -> cJSON* { return this->executeCommand(req); });

  cJSON const* settings = cJSON_GetObjectItem(conf, "settings");
  if (settings)
  {
    cJSON const* commands = cJSON_GetObjectItem(settings, "commands");
    if (commands)
      m_commands = cJSON_Duplicate(commands, true);
  }
}

cJSON*
ShellService::executeCommand(cJSON const* req)
{
  char const* commandName = JsonRpc::getString(req, "/params/command_name", true);

  // TODO: req should be able to pass in command line arguments
  // ${name} should be replaced with /params/args/name's value

  cJSON* res = nullptr;
  cJSON const* methodInfo = findCommand(m_commands, commandName);

  if (!methodInfo)
  {
    res = JsonRpc::makeError(-1, "can't find configuration for shell command '%s'",
      commandName);
  }
  else
  {
    res = invokeShellCommand(methodInfo,
      JsonRpc::search(req, "/params/args", false));
  }

  return res;
}
