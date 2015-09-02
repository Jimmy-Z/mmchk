
#ifndef EBML_H
#define EBML_H

#include "stdio_f64.h"
#include "basetype.h"

int EBML_get_length(UI8 b);
int EBML_dump_data(FILE* f, UI8* buffer);
int EBML_get_data(FILE* f, UI8* buffer);
SI64 EBML_parse_number(UI8* buffer, int length);
int EBML_get_number(FILE* f, SI64* number);

#endif

