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
#include "logger.h"

#include <glib.h>

static GKeyFile* key_file = g_key_file_new();
static char const* kDefaultGroup = "User";

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

int
appSettings_set(cJSON const* req, cJSON** res)
{
  if (!key_file)
  {
    // TODO
  }

  char const* group = jsonRpc_getString(req, "group");
  if (!group)
    group = kDefaultGroup;

  char const* name = jsonRpc_getString(req, "name");

  XLOG_JSON(logLevel_Debug, req);

  // TODO

  return jsonRpc_notImplemented(res);
}

int
appSettings_get(cJSON const* req, cJSON** res)
{
  if (!key_file)
  {
    // TODO
  }

  char const* group = jsonRpc_getString(req, "group");
  char const* name = jsonRpc_getString(req, "name");

  XLOG_JSON(logLevel_Debug, req);

  // TODO

  return jsonRpc_notImplemented(res);
}
