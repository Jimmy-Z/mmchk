all: mmchk

mmchk: main.c mmchk.c mmchk.h ebml.c ebml.h basetype.h stdio_f64.h
	cc -DDEBUG main.c mmchk.c ebml.c -o mmchk
