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
#ifndef __BLE_CLASS_H__
#define __BLE_CLASS_H__

#include <stdint.h>
#include <string>

enum class BleMajorDeviceClass : uint32_t
{
  AudioVideo = 0x00000400,
  Computer = 0x00000100,
  Health = 0x00000900,
  Imaging = 0x00000600,
  Misc = 0x00000000,
  Networking = 0x00000300,
  Peripheral = 0x00000500,
  Phone = 0x00000200,
  Toy = 0x00000800,
  Uncategorized = 0x00001f00,
  Wearable = 0x00000700
};

enum class BleMinorDeviceClass : uint32_t
{
  AudioVideoCamcorder = 0x00000434,
  AudioVideoCarAudio = 0x00000420,
  AudioVideoHandsFree = 0x00000408,
  AudioVideoHeadphones = 0x00000418,
  AudioVideoHiFiAudio = 0x00000428,
  AudioVideoLoudspeaker = 0x00000414,
  AudioVideoMicrophone = 0x00000410,
  AudioVideoPortableAudio = 0x0000041c,
  AudioVideoSetTopBox = 0x00000424,
  AudioVideoUncatoegorized = 0x00000400,
  AudioVideoVCR = 0x0000042c,
  AudioVideoVideoCamera = 0x00000430,
  AudioVideoVideoConferencing = 0x00000440,
  AudioVideoVideoDisplayAndLoudspeaker = 0x0000043c,
  AudioVideoVideoGamingToy = 0x00000448,
  AudioVideoVideoMonitor = 0x00000438,
  AudioVideoWearableHeadset = 0x00000404,
  ComputerDesktop = 0x00000104,
  ComputerHanheldPcPda = 0x00000110,
  ComputerLaptop = 0x0000010c,
  ComputerPalmSizePcPda = 0x00000114,
  ComoputerServer = 0x00000108,
  ComputerUncatorized = 0x00000100,
  ComputerWearable = 0x00000118,
  HealthBloodPressure = 0x00000904,
  HealthDataDisplay = 0x0000091c,
  HealthGlucose = 0x00000910,
  HealthPulseOximeter = 0x00000914,
  HealthPulseRate = 0x00000918,
  HealthThermometer = 0x00000908,
  HealthUncatoegorized = 0x00000900,
  HealthWeighing = 0x0000090c,
  PhoneCellular = 0x00000204,
  PhoneCordless = 0x00000208,
  PhoneIsdn = 0x00000214,
  PhoneModemOrGateway = 0x00000210,
  PhoneSmart = 0x0000020c,
  PhoneUncatogorized = 0x00000200,
  ToyController = 0x00000810,
  ToyDollActionFigure = 0x0000080c,
  ToyGame = 0x00000814,
  ToyRobot = 0x00000804,
  ToyUncatoegorized = 0x00000800,
  ToyVehicle = 0x00000808,
  WearableGlasses = 0x00000714,
  WearableHelmet = 0x00000710,
  WearableJacket = 0x0000070c,
  WearablePager = 0x00000708,
  WearbleUncatoegorized = 0x00000700,
  WerableWristWatch = 0x00000704
};

enum class BleMajorServiceClass : uint32_t
{
  Audio = 0x00200000,
  Capture = 0x00080000,
  Information = 0x00800000,
  LmitedDiscoverability = 0x00002000,
  Networking = 0x00020000,
  ObjectTransfer = 0x00100000,
  Positioning = 0x00010000,
  Render = 0x00040000,
  Telephony = 0x00400000
};

inline uint32_t operator | (BleMajorServiceClass c1, BleMajorServiceClass c2)
{
  uint32_t u1 = static_cast<uint32_t>(c1);
  uint32_t u2 = static_cast<uint32_t>(c2);
  return (u1 | u2);
}

inline uint32_t operator | (uint32_t c1, BleMajorServiceClass c2)
{
  uint32_t u2 = static_cast<uint32_t>(c2);
  return (c1 | u2);
}

std::string
bleCodToString(
  BleMajorDeviceClass majorDeviceClass,
  BleMinorDeviceClass minorDeviceClass,
  uint32_t majorServiceClass);

#endif

