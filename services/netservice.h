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
#ifndef __WPA_CONTROL_H__
#define __WPA_CONTROL_H__

#include "../defs.h"
#include "../rpcserver.h"

class NetService : public BasicRpcService
{
public:
  NetService();
  virtual ~NetService();
  virtual void init(cJSON const* conf, RpcNotificationFunction const& callback) override;
private:
  cJSON* getInterfaces(cJSON const* req);
};

#endif
