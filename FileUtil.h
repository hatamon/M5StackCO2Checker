#ifndef _FileUtil_h_
#define _FileUtil_h_

#include <M5Stack.h>

class FileUtil
{
  public:
    static int ReadInt(File& file);
};

#endif