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
#include "bleclass.h"

std::string
bleCodToString(
  BleMajorDeviceClass majorDeviceClass,
  BleMinorDeviceClass minorDeviceClass,
  uint32_t majorServiceClass)
{
  char buff[8] = { 0 };

  uint32_t cod = 0; // first 2 bits unused (format type)
  cod |= static_cast<uint32_t>(minorDeviceClass); // 6 bits
  cod |= static_cast<uint32_t>(majorDeviceClass); // 5 bits
  cod |= majorServiceClass;

  snprintf(buff, sizeof(buff), "%06x", cod);
  return std::string(buff);
}

#if 0
int main()
{
  uint32_t services =
    BleMajorServiceClass::Networking |
    BleMajorServiceClass::Capture |
    BleMajorServiceClass::ObjectTransfer |
    BleMajorServiceClass::Audio |
    BleMajorServiceClass::Telephony;

  std::string s = bleCodToString(
    BleMajorDeviceClass::Phone,
    BleMinorDeviceClass::PhoneSmart,
    services);

  std::cout << s << std::endl;
  return 0;
}
#endif
