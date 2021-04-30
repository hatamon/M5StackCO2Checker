#include "FileUtil.h"

int FileUtil::ReadInt(File& file)
{
    String s = "0";
    while (file.available())
    {
      char ch = file.read();
      if (ch == ',') break;
      if (ch == '\r') continue;
      if (ch == '\n') break;
      s.concat(ch);
    }

    return s.toInt();
}