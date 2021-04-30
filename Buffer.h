#ifndef _Buffer_h_
#define _Buffer_h_

#include "CO2Data.h"

class Buffer
{
  private:
    unsigned int _maxSize;
    unsigned int _startPos;
    unsigned int _size;
    CO2Data* _list;

  private:
    void SaveCO2BufferToFile(String filePath);
    String GetTmpFilePath(String filePath);

  public:
    Buffer(unsigned int maxSize);
    void Clear();
    void Append(CO2Data value);
    unsigned int GetSize();
    CO2Data Get(unsigned int index);
    void LoadCO2Buffer(String filePath);
    void SaveCO2Buffer(String filePath);
};

#endif