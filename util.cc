#include "util.h"

std::vector <std::string>
split(std::string const& str, std::string const& delim)
{
  std::vector <std::string> tokens;
  size_t prev = 0, pos = 0;
  do
  {
    pos = str.find(delim, prev);
    if (pos == std::string::npos) pos = str.length();
    std::string token = str.substr(prev, pos - prev);
    if (!token.empty()) tokens.push_back(token);
    prev = pos + delim.length();
  } while (pos < str.length() && prev < str.length());
  return tokens;
}


std::string runCommand(char const* command)
{
  FILE* fp;
  char buffer[256];
  fp = popen(command, "r");
  if (fp == NULL)
  {
    return std::string("unkown");
  }
  fgets(buffer, sizeof(buffer) - 1, fp);
  pclose(fp);
  return std::string(buffer);
}