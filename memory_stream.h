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
#include <sstream>
#include <mutex>
#include <queue>
#include <string.h>

#ifndef __MEMORY_STREAM_H__
#define __MEMORY_STREAM_H__

class memory_stream
{
public:
  memory_stream(char delim)
    : m_stream()
    , m_mutex()
    , m_delimiter(delim)
  {
  }


  int get_line(char* s, int n)
  {
    int bytes_read = 0;

    std::lock_guard<std::mutex> guard(m_mutex);
    while (!m_stream.empty() && (bytes_read < n))
    {
      s[bytes_read++] = m_stream.front();
      m_stream.pop();
    }

    return bytes_read;
  }

  void put_line(char const* s, int n)
  {
    if (!s)
      return;

    std::lock_guard<std::mutex> guard(m_mutex);
    for (int i = 0; i < n; ++i)
      m_stream.push(s[i]);
    m_stream.push(m_delimiter);
  }

  int size() const
  {
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_stream.size();
  }

private:
  bool read_complete(int bytes_read, int n)
  {
    if (m_stream.empty())
      return true;

    if (bytes_read == n)
      return true;

    if (m_stream.front() == m_delimiter)
      return true;

    return false;
  }

private:
  std::queue<char>    m_stream;
  mutable std::mutex  m_mutex;
  char                m_delimiter;
};

#endif
