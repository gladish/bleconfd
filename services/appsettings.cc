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
#include <string.h>
#include <sstream>

namespace
{
  GKeyFile* keyFile = g_key_file_new();

  std::pair<std::string, std::string>
  splitKey(cJSON const* req)
  {
    char const* s = JsonRpc::getString(req, "/params/key", true);
    char const* dot = strchr(s, '.');
    if (!dot)
    {
      std::stringstream buff;
      buff << "Invalid key path:'";
      buff << s;
      buff << "'";
      throw std::runtime_error(buff.str());
    }

    std::string section(s, (dot - s));
    std::string name(dot + 1);

    return std::pair<std::string, std::string>(section, name);
  }

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

  m_config_file = JsonRpc::getString(conf, "/settings/db-file", true);

  g_autoptr(GError) error = nullptr;
  if (!g_key_file_load_from_file(keyFile, m_config_file.c_str(), flags, &error))
  {
    if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
    {
      XLOG_ERROR("failed to load settings file %s. %s", m_config_file.c_str(), error->message);
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
  cJSON* res = nullptr;
  std::pair<std::string, std::string> key = splitKey(req);

  g_autoptr(GError) error = nullptr;
  g_autofree gchar* value = g_key_file_get_value(keyFile, key.first.c_str(), key.second.c_str(), &error);
  if (!value)
  {
    if (!g_error_matches(error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND) &&
        !g_error_matches(error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND))
    {
      res = JsonRpc::makeError(error->code, "Error invoking g_key_file_get_value:%s",
        error->message);
    }
    else
    {
      res = JsonRpc::makeError(-1, "%s.%s not found", key.first.c_str(), key.second.c_str());
    }
  }
  else
  {
    cJSON_AddItemToObject(res, "name",  cJSON_CreateString(JsonRpc::getString(req, "/params/key", true)));
    cJSON_AddItemToObject(res, "value", cJSON_CreateString(value));
  }

  return res;
}

cJSON*
AppSettingsService::set(cJSON const* req)
{
  cJSON* res = nullptr;

  char const* value = JsonRpc::getString(req, "/params/value", false);
  if (!value)
  {
    // TODO:
    // clear value
  }
  else
  {
    std::pair<std::string, std::string> key = splitKey(req);
    g_autoptr(GError) error = nullptr;

    g_key_file_set_value(keyFile, key.first.c_str(), key.second.c_str(), value);
    if (!g_key_file_save_to_file(keyFile, m_config_file.c_str(), &error))
    {
      XLOG_WARN("failed to save config file %s. %s", m_config_file.c_str(),
        error->message);
      res = JsonRpc::makeError(error->code, "g_key_file_save_file:%s", error->message);
    }
    else
    {
      res = cJSON_CreateNumber(0);
    }
  }

  return res;
}

cJSON*
AppSettingsService::getStatus(cJSON const* UNUSED_PARAM(req))
{
  g_autofree gchar* value = configGetString("system", "provision_status", "unprovisioned");
  cJSON* res = cJSON_CreateObject();
  cJSON_AddItemToObject(res, "provision-status", cJSON_CreateString(value));
  return res;
}
