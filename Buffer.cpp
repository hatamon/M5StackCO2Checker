#include "Buffer.h"
#include "FileUtil.h"

void Buffer::SaveCO2BufferToFile(String filePath)
{
  if (SD.exists(filePath.c_str()))
  {
    SD.remove(filePath.c_str());
  }

  File f = SD.open(filePath.c_str(), FILE_WRITE);
  if (!f)
  {
    return;
  }
  
  String s = String(_maxSize) + "," + String(_startPos) + "," + String(_size);
  f.println(s);
  
  for (int i = 0; i < _maxSize; i++)
  {
    _list[i].SerializeToFile(f);
  }

  f.close();
}

String Buffer::GetTmpFilePath(String filePath)
{
  return filePath + "tmp";
}

Buffer::Buffer(unsigned int maxSize)
{
  _maxSize = maxSize;
  _list = nullptr;
  Clear();
}

void Buffer::Clear()
{
  _startPos = 0;
  _size = 0;
  if (_list != nullptr)
  {
    delete [] _list;
  }

  _list = new CO2Data[_maxSize];
}

void Buffer::Append(CO2Data& value)
{
  unsigned int nextPos = 0;
  if (_size < _maxSize) {
    _startPos = 0;
    nextPos = _size;
    _size++;
  }
  else {
    nextPos = _startPos + 1;
    if (nextPos >= _maxSize) {
      nextPos = 0;
    }
    _startPos++;
    if (_startPos >= _maxSize) {
      _startPos = 0;
    }
  }
  _list[nextPos] = value;
}

unsigned int Buffer::GetSize()
{
  return _size;
}

CO2Data Buffer::Get(unsigned int index)
{
  if (index >= GetSize())
  {
    CO2Data empty;
    return empty;
  }

  unsigned int realIndex = (_startPos + index) % _maxSize;
  return _list[realIndex];
}

void Buffer::LoadCO2Buffer(String filePath)
{
  Clear();
  
  auto tmpFilePath = GetTmpFilePath(filePath);
  auto loadFilePath = SD.exists(tmpFilePath.c_str()) ? tmpFilePath : filePath;

  if (!SD.exists(loadFilePath.c_str()))
  {
    return;
  }

  File f = SD.open(loadFilePath.c_str(), FILE_READ);
  if (!f)
  {
    return;
  }
  
  _maxSize = FileUtil::ReadInt(f);
  _startPos = FileUtil::ReadInt(f);
  _size = FileUtil::ReadInt(f);
  
  for (int i = 0; i < _maxSize; i++)
  {
    _list[i].DeserializeFromFile(f);
  }

  f.close();
}

void Buffer::SaveCO2Buffer(String filePath)
{
  auto tmpFilePath = GetTmpFilePath(filePath);
  SaveCO2BufferToFile(tmpFilePath);
  SaveCO2BufferToFile(filePath);
  SD.remove(tmpFilePath.c_str());
}