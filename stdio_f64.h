
#pragma once

#ifndef WIN32
#define __USE_LARGEFILE64
//#define __USE_LARGEFILE
//#define _FILE_OFFSET_BITS 64
//#define _LARGEFILE_SOURCE
//#define _LARGEFILE64_SOURCE
#endif

#include <stdio.h>

#ifdef WIN32
#define ftell(f) _ftelli64(f)
#define fseek(f, o, c) _fseeki64(f, o, c)
#else
//add 64bit ftell/fseek equivalent for other platform
#define fopen(f, m) fopen64(f, m)
#define ftell(f) ftello64(f)
#define fseek(f, o, c) fseeko64(f, o, c)
#endif
