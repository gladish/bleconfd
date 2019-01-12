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
  char const* kDefaultGroupName = "user";

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

  enum class DynamicPropertyOperation
  {
    Get,
    Set
  };

  cJSON*
  exec(cJSON const* req, cJSON const* conf, DynamicPropertyOperation op)
  {
    cJSON* res = nullptr;

    char const* key = JsonRpc::getString(req, "/params/key", true);
    XLOG_INFO("executing command for setting %s", key);

    std::string cmdline = JsonRpc::getString(conf, "exec", true);
    if (op == DynamicPropertyOperation::Get)
    {
      cmdline += " get ";
      cmdline += key;
    }
    else
    {
      char const* value = JsonRpc::getString(req, "/params/value", true);
      cmdline += " set ";
      cmdline += key;
      cmdline + " ";
      cmdline + value;
    }

    g_autofree gchar* out = nullptr;
    g_autofree gchar* err = nullptr;
    gint exit_status = 0;
    g_autoptr(GError) error = nullptr;
    gboolean b = g_spawn_command_line_sync(cmdline.c_str(), &out, &err, &exit_status, &error);

    XLOG_INFO("ret:%d", b);
    XLOG_INFO("out:%s", out);
    XLOG_INFO("err:%s", err);
    XLOG_INFO("status:%d", exit_status);

    if (error)
      XLOG_INFO("error:%d", error->code);

    if (b && out && (exit_status == 0))
    {
      if (op == DynamicPropertyOperation::Get)
      {
        std::string value = chomp(out);
        res = cJSON_CreateString(value.c_str());
      }
      else
      {
        res = cJSON_CreateNumber(0);
      }
    }
    else
    {
      int code = exit_status;
      if (code == 0)
        code = -1;

      char const* message = nullptr;
      if (error)
        message = error->message;

      res = JsonRpc::makeError(code, "failed to execute %s. %s", cmdline.c_str(), message);
    }

    return res;
  }
}

JSONRPC_SERVICE_DEFINE(config, []{return new AppSettingsService();});

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
  registerMethod("get-keys", [this](cJSON const* req) -> cJSON* { return this->getKeys(req); });
}

cJSON const*
AppSettingsService::getDynamicConfig(char const* s) const
{
  XLOG_INFO("checking for dynamic property %s", s);

  cJSON const* dynamicProperties = JsonRpc::search(m_config, "/settings/dynamic_properties", false);
  if (dynamicProperties)
  {
    XLOG_DEBUG("checking dynamic properties");
    for (int i = 0, n = cJSON_GetArraySize(dynamicProperties); i < n; ++i)
    {
      cJSON const* prop = cJSON_GetArrayItem(dynamicProperties, i);
      cJSON const* name = cJSON_GetObjectItem(prop, "name");
      if (name && strcmp(name->valuestring, s) == 0)
        return prop;
    }
  }
  else
  {
    char* s = cJSON_Print(m_config);
    XLOG_DEBUG("no dynamic properties configured");
    XLOG_DEBUG("%s", s);
    free(s);
  }
  return nullptr;
}

cJSON*
AppSettingsService::getKeys(cJSON const* UNUSED_PARAM(req))
{
  cJSON* res = cJSON_CreateArray();

  cJSON const* dynamicProperties = JsonRpc::search(m_config, "/settings/dynamic_properties", false);
  if (dynamicProperties)
  {
    for (int i = 0, n = cJSON_GetArraySize(dynamicProperties); i < n; ++i)
    {
      cJSON const* prop = cJSON_GetArrayItem(dynamicProperties, i);
      cJSON const* name = cJSON_GetObjectItem(prop, "name");
      if (name)
        cJSON_AddItemToArray(res, cJSON_CreateString(name->valuestring));
    }
  }

  gsize n = 0;
  g_autoptr(GError) error = nullptr;
  g_autofree gchar** keys = g_key_file_get_keys(keyFile, kDefaultGroupName, &n, &error);
  if (keys)
  {
    for (int i = 0; i < static_cast<int>(n); ++i)
      cJSON_AddItemToArray(res, cJSON_CreateString(keys[i]));
  }

  return res;
}

cJSON*
AppSettingsService::get(cJSON const* req)
{
  cJSON* res = nullptr;

  char const* key = JsonRpc::getString(req, "/params/key", true);

  cJSON const* conf = getDynamicConfig(key);
  if (conf)
  {
    res = exec(req, conf, DynamicPropertyOperation::Get);
  }
  else
  {
    g_autoptr(GError) error = nullptr;
    g_autofree gchar* value = g_key_file_get_value(keyFile, kDefaultGroupName, key, &error);
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
        res = JsonRpc::makeError(-1, "%s not found", key);
      }
    }
    else
    {
      cJSON_AddItemToObject(res, "name",  cJSON_CreateString(JsonRpc::getString(req, "/params/key", true)));
      cJSON_AddItemToObject(res, "value", cJSON_CreateString(value));
    }
  }

  return res;
}

cJSON*
AppSettingsService::set(cJSON const* req)
{
  cJSON* res = nullptr;

  char const* key = JsonRpc::getString(req, "/params/key", true);

  cJSON const* conf = getDynamicConfig(key);
  if (conf)
  {
    res = exec(req, m_config, DynamicPropertyOperation::Set);
  }
  else
  {
    char const* key = JsonRpc::getString(req, "/params/key", true);
    char const* value = JsonRpc::getString(req, "/params/value", true);

    g_autoptr(GError) error = nullptr;
    g_key_file_set_value(keyFile, kDefaultGroupName, key, value);
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
