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
#include <string.h>
#include <stdio.h>
#include <map>

#include <sstream>


static GKeyFile* key_file = g_key_file_new();
static char* settings_file_path = nullptr;
static char const* kDefaultGroup = "User";
static char const* kBLE = "ble";
static char const* kWifi = "wlan";

/**
 * return file not exist error
 * @param result the response
 * @return the result code
 */
int
file_not_exist(cJSON** result)
{
  return jsonRpc_makeError(result, ENOENT, "settings file not exist");
}

/**
 * return group not exist error
 * @param result the response
 * @return the result code
 */
int
group_not_exist(cJSON** result, char const* group)
{
  return jsonRpc_makeError(result, ENOENT, "group with name %s not exist", group);
}

/**
 * return key not exist error
 * @param result the response
 * @return the result code
 */
int
key_not_exist(cJSON** result, char const* group, char const* key)
{
  return jsonRpc_makeError(result, ENOENT, "key with name %s not exist in group %s", key, group);
}

/**
 * write full settings file content to filesystem
 * @return the result code
 */
int
flush_to_filesystem()
{
  g_autoptr(GError) error = nullptr;
  gsize data_len;
  gchar* content = g_key_file_to_data(key_file, &data_len, &error);
  if (error)
  {
    XLOG_ERROR("get data from key file failed");
    return error->code;
  }

  gboolean write_ret = g_file_set_contents(settings_file_path, content, -1, &error);
  if (error || !write_ret)
  {
    XLOG_ERROR("write data to file, %s", error->message);
    return error->code;
  }
  return 0;
}

/**
 * rollback all changes
 */
void
roll_back()
{
  g_autoptr(GError) error = nullptr;
  GKeyFile* kf = g_key_file_new();
  if (!g_key_file_load_from_file(kf, settings_file_path, G_KEY_FILE_KEEP_COMMENTS, &error))
  {
    g_key_file_free(kf);
    XLOG_ERROR("roll back failed, settings key file now contains dirty data, %s", error->message);
    return;
  }
  g_key_file_free(key_file);
  key_file = kf;
}

/**
 * check key name is illegal
 */
gboolean
g_key_file_is_key_name (const gchar *name)
{
  gchar *p, *q;

  if (name == NULL)
    return FALSE;

  p = q = (gchar *) name;
  /* We accept a little more than the desktop entry spec says,
   * since gnome-vfs uses mime-types as keys in its cache.
   */
  while (*q && *q != '=' && *q != '[' && *q != ']')
    q = g_utf8_find_next_char (q, NULL);

  /* No empty keys, please */
  if (q == p)
    return FALSE;

  /* We accept spaces in the middle of keys to not break
   * existing apps, but we don't tolerate initial or final
   * spaces, which would lead to silent corruption when
   * rereading the file.
   */
  if (*p == ' ' || q[-1] == ' ')
    return FALSE;

  if (*q == '[')
    {
      q++;
      while (*q && (g_unichar_isalnum (g_utf8_get_char_validated (q, -1)) || *q == '-' || *q == '_' || *q == '.' || *q == '@'))
        q = g_utf8_find_next_char (q, NULL);

      if (*q != ']')
        return FALSE;

      q++;
    }

  if (*q != '\0')
    return FALSE;

  return TRUE;
}

/**
 * check the group name is illegal
 */
gboolean
g_key_file_is_group_name (const gchar *name)
{
  gchar *p, *q;

  if (name == NULL)
    return FALSE;

  p = q = (gchar *) name;
  while (*q && *q != ']' && *q != '[' && !g_ascii_iscntrl (*q))
    q = g_utf8_find_next_char (q, NULL);

  if (*q != '\0' || q == p)
    return FALSE;

  return TRUE;
}

int
appSettings_init(char const* settings_file)
{
  GKeyFileFlags flags = G_KEY_FILE_KEEP_COMMENTS;

  size_t file_name_len = strlen(settings_file);
  settings_file_path = static_cast<char*>(malloc(file_name_len + 1));
  memcpy(settings_file_path, settings_file, file_name_len);
  settings_file_path[file_name_len] = '\0';

  g_autoptr(GError) error = nullptr;
  if (!g_key_file_load_from_file(key_file, settings_file, flags, &error))
  {
    if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
    {
      XLOG_ERROR("failed to load settings file %s. %s", settings_file,
                 error->message);
      return error->code;
    } else
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
    XLOG_ERROR("%s, appSettings_get_%s_value failed, key = %s, section = %s", error->message, section, key, section);
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

/**
 * delete key-value for ini file
 */
int
appSettings_delete(cJSON const* req, cJSON** res)
{
  if (!key_file)
  {
    return file_not_exist(res);
  }

  char const* group = jsonRpc_getString(req, "group", true);
  char const* name = jsonRpc_getString(req, "name", true);

  if (!g_key_file_has_group(key_file, group))
  {
    return group_not_exist(res, group);
  }

  g_autoptr(GError) error = nullptr;
  if (!g_key_file_has_key(key_file, group, name, &error))
  {
    return key_not_exist(res, group, name);
  }

  error = nullptr;
  g_key_file_remove_key(key_file, group, name, &error);
  if (error)
  {
    return jsonRpc_makeError(res, error->code, "remove failed, %s", error->message);
  }

  int ret = flush_to_filesystem();
  if (ret)  // write failed, roll back to keep memory key_file content same as ini file, for prevent bugs
  {
    roll_back();
    return jsonRpc_makeError(res, ret, "delete failed, change has been rollback");
  }

  return jsonRpc_makeResultValue(res, RETURN_OK, "value has been delete successful");
}

/**
 * add/edit key-value for group
 */
int
appSettings_set(cJSON const* req, cJSON** res)
{
  if (!key_file)
  {
    return file_not_exist(res);
  }

  char const* group = jsonRpc_getString(req, "group", true);
  char const* name = jsonRpc_getString(req, "name", true);
  char const* value = jsonRpc_getString(req, "value", true);

  if (!g_key_file_has_group(key_file, group))
  {
    return group_not_exist(res, group);
  }

  if (!g_key_file_is_group_name(name) || !g_key_file_is_key_name(name))
  {
    return jsonRpc_makeError(res, EACCES, "illegal key name = %s", name);
  }

  // set value
  g_key_file_set_value(key_file, group, name, value);

  int ret = flush_to_filesystem();
  if (ret)  // write failed, roll back to keep memory key_file content same as ini file, for prevent bugs
  {
    roll_back();
    return jsonRpc_makeError(res, ret, "save settings failed, change has been rollback");
  }

  return jsonRpc_makeResultValue(res, RETURN_OK, "value has been set successful");
}

/**
 * get value by key and group
 */
int
appSettings_get(cJSON const* req, cJSON** res)
{
  if (!key_file)
  {
    return file_not_exist(res);
  }

  char const* group = jsonRpc_getString(req, "group", true);
  char const* name = jsonRpc_getString(req, "name", true);

  if (!g_key_file_has_group(key_file, group))
  {
    return group_not_exist(res, group);
  }

  char const* value = appSettings_get_string_value(name, group);
  if (value == nullptr)
  {
    return key_not_exist(res, group, name);
  }

  *res = cJSON_CreateObject();
  cJSON_AddItemToObject(*res, "value", cJSON_CreateString(value));

  return 0;
}

/**
 * get all values group by grou name
 */
int
appSettings_get_all(cJSON const* /*req*/, cJSON** res)
{
  if (!key_file)
  {
    return file_not_exist(res);
  }

  cJSON* result = cJSON_CreateObject();

  gsize group_len;
  gchar** groups = g_key_file_get_groups(key_file, &group_len);
  for (gsize i = 0; i < group_len; ++i)
  {

    cJSON* group = cJSON_CreateObject();
    gsize keys_count;
    g_autoptr(GError) error = nullptr;
    gchar** keys = g_key_file_get_keys(key_file, groups[i], &keys_count, &error);
    if (error)
    {
      XLOG_ERROR("try to get all keys from group %s, but failed, skip this group", groups[i]);
      continue;
    }

    for (gsize j = 0; j < keys_count; ++j)
    {
      gchar* value = g_key_file_get_string(key_file, groups[i], keys[j], &error);
      if (error)
      {
        XLOG_ERROR("try to get value from group %s, ket = %s, but failed, skip this key", groups[i], keys[j]);
        continue;
      }
      cJSON_AddItemToObject(group, keys[j], cJSON_CreateString(value));
      g_free(value);
    }
    cJSON_AddItemToObject(result, groups[i], group);
    g_free(keys);
  }

  g_free(groups);
  *res = result;
  return 0;
}

/**
 * add group
 */
int
appSettings_add_group(cJSON const* req, cJSON** res)
{
  if (!key_file)
  {
    return file_not_exist(res);
  }
  char const* group = jsonRpc_getString(req, "group", true);

  if (g_key_file_has_group(key_file, group))
  {
    return jsonRpc_makeError(res, EEXIST, "group with name %s already exist", group);
  }

  if (!g_key_file_is_group_name(group))
  {
    return jsonRpc_makeError(res, EACCES, "illegal group name = %s", group);
  }


  g_key_file_set_string(key_file, group, kDefaultGroup, kDefaultGroup);
  g_autoptr(GError) error = nullptr;
  g_key_file_remove_key(key_file, group, kDefaultGroup, &error);
  if (error)
  {
    roll_back();
    return jsonRpc_makeError(res, error->code, "add group failed, %s", error->message);
  }


  int ret = flush_to_filesystem();
  if (ret)  // write failed, roll back to keep memory key_file content same as ini file, for prevent bugs
  {
    roll_back();
    return jsonRpc_makeError(res, ret, "save settings failed, change has been rollback");
  }

  return jsonRpc_makeResultValue(res, RETURN_OK, "group has been create successful");
}


/**
 * delete group from ini file
 */
int
appSettings_delete_group(cJSON const* req, cJSON** res)
{
  if (!key_file)
  {
    return file_not_exist(res);
  }

  char const* group = jsonRpc_getString(req, "group", true);
  if (!g_key_file_has_group(key_file, group))
  {
    return group_not_exist(res, group);
  }

  g_autoptr(GError) error = nullptr;
  g_key_file_remove_group(key_file, group, &error);
  if (error)
  {
    return jsonRpc_makeError(res, error->code, "remove group failed, %s", error->message);
  }

  int ret = flush_to_filesystem();
  if (ret)  // write failed, roll back to keep memory key_file content same as ini file, for prevent bugs
  {
    roll_back();
    return jsonRpc_makeError(res, ret, "save settings failed, change has been rollback");
  }
  return jsonRpc_makeResultValue(res, RETURN_OK, "group has been delete successful");
}

/**
 * modify group name
 */
int
appSettings_modify_group(cJSON const* req, cJSON** res)
{
  if (!key_file)
  {
    return file_not_exist(res);
  }

  char const* origin_group = jsonRpc_getString(req, "originGroup", true);
  char const* new_group = jsonRpc_getString(req, "newGroup", true);

  if (!strcmp(origin_group, new_group))
  {
    return jsonRpc_makeError(res, ENOTSUP, "new group name cannot be same as origin group name");
  }

  // origin group name not exist
  if (!g_key_file_has_group(key_file, origin_group))
  {
    return group_not_exist(res, origin_group);
  }

  // new group name already exist
  if (g_key_file_has_group(key_file, new_group))
  {
    return jsonRpc_makeError(res, EEXIST, "new group name already exist");
  }

  // check new group
  if (!g_key_file_is_group_name(new_group))
  {
    return jsonRpc_makeError(res, EACCES, "illegal group name = %s", new_group);
  }

  // backup all old key values
  std::map<gchar*, gchar*> key_values;
  g_autoptr(GError) error = nullptr;
  gsize keys_count;

  gchar** keys = g_key_file_get_keys(key_file, origin_group, &keys_count, &error);
  if (error)
  {
    XLOG_ERROR("try to get all keys from group %s", origin_group);
    return jsonRpc_makeError(res, error->code, "%s", error->message);
  }

  for (gsize j = 0; j < keys_count; ++j)
  {
    error = nullptr;
    gchar* value = g_key_file_get_string(key_file, origin_group, keys[j], &error);
    if (error)
    {
      XLOG_ERROR("try to get value from group %s, ket = %s, but failed, skip this key", origin_group, keys[j]);
      continue;
    }
    key_values[keys[j]] = value;
  }

  // remove old group
  int ret = 0;
  cJSON * mock_req = cJSON_CreateObject();
  cJSON * params = cJSON_CreateObject();
  cJSON_AddItemToObject(params, "group", cJSON_CreateString(origin_group));
  cJSON_AddItemToObject(mock_req, "params", params);
  ret = appSettings_delete_group(mock_req, res);
  cJSON_Delete(mock_req);
  if (ret)
  {
    return ret;
  }


  //create new group
  mock_req = cJSON_CreateObject();
  params = cJSON_CreateObject();
  cJSON_AddItemToObject(params, "group", cJSON_CreateString(new_group));
  cJSON_AddItemToObject(mock_req, "params", params);
  ret = appSettings_add_group(mock_req, res);
  cJSON_Delete(mock_req);
  if (ret)
  {
    return ret;
  }

  // copy backup key values to new group
  for (std::map<gchar*, gchar*>::iterator it = key_values.begin(); it != key_values.end(); ++it)
  {
    g_key_file_set_string(key_file, new_group, it->first, it->second);
    g_free(it->first);
    g_free(it->second);
  }

  // write to disk
  ret = flush_to_filesystem();
  if (ret)  // write failed, roll back to keep memory key_file content same as ini file, for prevent bugs
  {
    roll_back();
    return jsonRpc_makeError(res, ret, "save settings failed, change has been rollback");
  }

  return jsonRpc_makeResultValue(res, RETURN_OK, "group has been rename successful");
}