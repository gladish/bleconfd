//
// Copyright [2018] [Comcast NBCUniversal]
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
#include "appsettings.h"
#include "../rpclogger.h"
#include "../jsonrpc.h"

#include <glib.h>
#include <sstream>

namespace
{
  GKeyFile* keyFile = g_key_file_new();

  gchar*
  configGetString(char const* section, char const* name, char const* defaultValue)
  {
    g_autoptr(GError) error = nullptr;
    gchar* value = g_key_file_get_string(keyFile, section, name, &error);
    if (!value)
    {
      if (!g_error_matches(error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND) &&
          !g_error_matches(error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND))
      {
        XLOG_WARN("g_key_file_get_string:%d", error->code);
        std::stringstream buff;
        buff << "Error invoking g_key_file_key_string. ";
        buff << error->message;
        throw std::runtime_error(buff.str());
      }
    }
    if (!value)
      value = g_strdup(defaultValue);
    return value;
  }
}

extern "C"
{
  RpcService*
  AppSettings_Create()
  {
    return new AppSettingsService();
  }
}

AppSettingsService::AppSettingsService()
  : BasicRpcService("config")
{
}

AppSettingsService::~AppSettingsService()
{
}

void
AppSettingsService::init(cJSON const* conf, RpcNotificationFunction const& callback)
{
  BasicRpcService::init(conf, callback);

  GKeyFileFlags flags = G_KEY_FILE_KEEP_COMMENTS;

  std::string configFile = JsonRpc::getString(conf, "/settings/db-file", true);

  g_autoptr(GError) error = nullptr;
  if (!g_key_file_load_from_file(keyFile, configFile.c_str(), flags, &error))
  {
    if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
    {
      XLOG_ERROR("failed to load settings file %s. %s", configFile.c_str(), error->message);
    }
    else
    {
      keyFile = g_key_file_new();
    }
  }

  registerMethod("get", [this](cJSON const* req) -> cJSON* { return this->get(req); });
  registerMethod("set", [this](cJSON const* req) -> cJSON* { return this->set(req); });
  registerMethod("get-status", [this](cJSON const* req) -> cJSON* { return this->getStatus(req); });
}

cJSON*
AppSettingsService::get(cJSON const* req)
{
  return JsonRpc::notImplemented("get");
}

cJSON*
AppSettingsService::set(cJSON const* req)
{
  return JsonRpc::notImplemented("set");
}

cJSON*
AppSettingsService::getStatus(cJSON const* req)
{
  g_autofree gchar* value = configGetString("system", "provision_status", "unprovisioned");
  cJSON* res = cJSON_CreateObject();
  cJSON_AddItemToObject(res, "provision-status", cJSON_CreateString(value));
  return res;
}
