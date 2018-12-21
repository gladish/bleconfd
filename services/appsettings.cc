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
#include "../jsonRpc.h"

#include <glib.h>

static GKeyFile* key_file = g_key_file_new();
static char const* kDefaultGroup = "User";
static char const* kBLE = "ble";
static char const* kWifi = "wlan";

extern "C"
{
  RpcService*
  AppSettings_Create()
  {
    return new AppSettingsService();
  }
}

AppSettingsService::AppSettingsService()
  : BasicRpcService("app-settings")
{
}

AppSettingsService::~AppSettingsService()
{
}

void
AppSettingsService::init(std::string const& configFile, RpcNotificationFunction const& callback)
{
  BasicRpcService::init(configFile, callback);

  GKeyFileFlags flags = G_KEY_FILE_KEEP_COMMENTS;

  g_autoptr(GError) error = nullptr;
  if (!g_key_file_load_from_file(key_file, configFile.c_str(), flags, &error))
  {
    if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
    {
      XLOG_ERROR("failed to load settings file %s. %s", configFile.c_str(), error->message);
    }
    else
    {
      key_file = g_key_file_new();
    }
  }

  registerMethod("get", [this](cJSON const* req) -> cJSON* { return this->get(req); });
  registerMethod("set", [this](cJSON const* req) -> cJSON* { return this->set(req); });
}

cJSON*
AppSettingsService::get(cJSON const* req)
{
  return notImplemented("get");
}

cJSON*
AppSettingsService::set(cJSON const* req)
{
  return notImplemented("set");
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
