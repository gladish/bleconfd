//
// Copyright [2018] [jacobgladish@yahoo.com]
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
#include "appSettings.h"
#include "jsonRpc.h"
#include "xLog.h"

#include <glib.h>

static GKeyFile* key_file = g_key_file_new();
static char const* kDefaultGroup = "User";
static char const* kBLE = "ble";
static char const* kWifi = "wlan";

static int appSettings_init(char const* settings_file);
static cJSON* appSettings_set(cJSON const* req);
static cJSON* appSettings_get(cJSON const* req);

AppSettingsService::AppSettingsService()
  : BasicRpcService("app-settings")
{
}

AppSettingsService::~AppSettingsService()
{
}

void
AppSettingsService::init(std::string const& configFile,
  RpcNotificationFunction const& UNUSED_PARAM(callback))
{
  appSettings_init(configFile.c_str());
  registerMethod("get", [this](cJSON const* req) -> cJSON* { return this->get(req); });
  registerMethod("set", [this](cJSON const* req) -> cJSON* { return this->set(req); });
}

cJSON*
AppSettingsService::get(cJSON const* req)
{
  return appSettings_get(req);
}

cJSON*
AppSettingsService::set(cJSON const* req)
{
  return appSettings_set(req);
}

int
appSettings_init(char const* settings_file)
{
  GKeyFileFlags flags = G_KEY_FILE_KEEP_COMMENTS;

  g_autoptr(GError) error = nullptr;
  if (!g_key_file_load_from_file(key_file, settings_file, flags, &error))
  {
    if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
    {
      XLOG_ERROR("failed to load settings file %s. %s", settings_file,
        error->message);
      return error->code;
    }
    else
    {
      key_file = g_key_file_new();
    }
  }

  return 0;
}

// get string value from local ini file
char const*
appSettings_get_string_value(char const* key, char const* section)
{
  g_autoptr(GError) error = nullptr;
  gchar* value = g_key_file_get_string(key_file,
                                       section,
                                       key,
                                       &error);
  if (error)
  {
    XLOG_ERROR("appSettings_get_%s_value failed, key = %s, section = %s", section, key, section);
    return nullptr;
  }
  return value;
}

char const*
appSettings_get_ble_value(char const* key)
{
  return appSettings_get_string_value(key, kBLE);
}

char const*
appSettings_get_wifi_value(char const* key)
{
  return appSettings_get_string_value(key, kWifi);
}


cJSON*
appSettings_set(cJSON const* req)
{
  if (!key_file)
  {
    // TODO
  }

  char const* group = jsonRpc_getString(req, "group", false);
  if (!group)
    group = kDefaultGroup;

//  char const* name = jsonRpc_getString(req, "name");

  XLOG_JSON(logLevel_Debug, req);

  // TODO

  cJSON* res = nullptr;
  jsonRpc_notImplemented(&res);
  return res;
}


cJSON*
appSettings_get(cJSON const* req)
{
  if (!key_file)
  {
    // TODO
  }

//  char const* group = jsonRpc_getString(req, "group");
//  char const* name = jsonRpc_getString(req, "name");

  XLOG_JSON(logLevel_Debug, req);

  // TODO

  cJSON* res = nullptr;
  jsonRpc_notImplemented(&res);
  return res;
}
