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
#include <sstream>
#include <mutex>
#include <string.h>

#ifndef __MEMORY_STREAM_H__
#define __MEMORY_STREAM_H__

class memory_stream
{
public:
  memory_stream(char delim)
    : m_size(0)
    , m_stream()
    , m_mutex()
    , m_delimiter(delim)
  {
  }


  int getline(char* s, int n)
  {
    int bytes_read = 0;

    std::lock_guard<std::mutex> guard(m_mutex);
    m_stream.getline(s, n, m_delimiter);
    if (m_stream.gcount() > 0)
      bytes_read = m_stream.gcount();
    m_stream.clear();

    return bytes_read;
  }

  void append(char const* s)
  {
    if (!s)
      return;

    std::lock_guard<std::mutex> guard(m_mutex);
    m_stream << s;
    m_stream << m_delimiter;
    m_size += (strlen(s) + 1);
  }

  int size() const
  {
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_size;
  }

private:
  int                 m_size;
  std::stringstream   m_stream;
  mutable std::mutex  m_mutex;
  char                m_delimiter;
};

#endif
