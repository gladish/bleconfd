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
#ifndef __APP_SETTINGS_H__
#define __APP_SETTINGS_H__

#include "defs.h"
#include "rpcserver.h"

class AppSettingsService : public RpcService
{
public:
  AppSettingsService();
  virtual ~AppSettingsService();

public:
  virtual void init(std::string const& configFile,
    RpcNotificationFunction const& callback) override;
  virtual std::string name() const override;
  virtual std::vector<std::string> methodNames() const override;
  virtual RpcMethod method(std::string const& name) const override;
};

// TODO: I don't like exposing this, but we use these as a service and for
// configuring the application

/**
 * get ble values
 * @param key  the key
 * @return  the value
 */
char const* appSettings_get_ble_value(char const* key);

/**
 * get wifi values
 * @param key  the key
 * @return  the value
 */
char const* appSettings_get_wifi_value(char const* key);

#endif
