
#include "ebml.h"

//EBML utility function, stupid but easy huh-_,-
int EBML_get_length(UI8 b)
{
  int r = 0;
  if(b >= 0x80)
    return 1;
  else if(b >= 0x40)
    return 2;
  else if(b >= 0x20)
    return 3;
  else if(b >= 0x10)
    return 4;
  else if(b >= 0x08)
    return 5;
  else if(b >= 0x04)
    return 6;
  else if(b >= 0x02)
    return 7;
  else
    return 8;
}

int EBML_dump_data(FILE* f, UI8* buffer)
{
  int i,length = EBML_get_length(buffer[0]);//must prepare the first byte in buffer first!
  for(i = 1; i < length; ++i)
  {
    int c = fgetc(f);
    if(c == EOF)
      return -1;//error reading file
    buffer[i] = c;
  }
  return length;
}

int EBML_get_data(FILE* f, UI8* buffer)
{
  int c = fgetc(f);
  if(c == EOF)
    return -1;//error reading file
  buffer[0] = c;
  return EBML_dump_data(f,buffer);
}

SI64 EBML_parse_number(UI8* buffer, int length)
{//no error check here, caution
  int i;
  SI64 number = 0;
#define p ((UI8*)&number)
  UI8 bitmask = ~(0x80 >> (--length));
  for(i = 0; i < length; ++i)
  {
    p[i] = buffer[length - i];
  }
  p[length] = buffer[0] & bitmask;
#undef p
  return number;
}

int EBML_get_number(FILE* f, SI64* number)
{
  int count;
  UI8 buffer[8];
  if((count = EBML_get_data(f, buffer)) == -1)
  {
    return -1;
  }
  *number = EBML_parse_number(buffer, count);
  return count;
}

