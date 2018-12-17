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

#ifndef __ECDH_H__
#define __ECDH_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define kPrivateKeyPath "/var/run/xsetupd/bootstrap_private.pem"
#define kPublicKeyPath "/var/run/xsetupd/bootstrap_public.pem"

int ECDH_DecryptWiFiSettings(cJSON const* server_reply, cJSON** wifi_settings);

#ifdef __cplusplus
}
#endif
#endif
