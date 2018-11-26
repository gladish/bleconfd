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
#ifndef __BLE_PACKET_H__
#define __BLE_PACKET_H__

#include <stdint.h>
#include <vector>

class BLEPacket
{
public:
  class Fragmenter
  {
  public:
    Fragmenter(char const* s, int n, int fragment_size);

  public:
    bool nextFrameWithHeader(uint8_t* buff, int* n);
    bool nextFrame(uint8_t const** p, int* n, uint32_t* header = nullptr);

  private:
    uint8_t const*  m_data;
    int             m_index;
    int             m_header;
    int             m_length;
    int             m_fragment_size;
  };

  class Assembler
  {
  public:
    Assembler();

  public:
    BLEPacket* appendData(uint8_t const* data, int n);

  private:
    std::vector<uint8_t *> m_data;
  };

public:
  static void printHeader(int n);
  static uint16_t frameLengthFromHeader(uint16_t n);
  static bool isContinuationFrame(uint32_t header) ;
  static void printFrame(uint8_t* frame);
};

#endif
