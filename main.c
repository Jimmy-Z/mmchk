#if WIN32
#include <stdlib.h>
#endif

#if (defined (DEBUG) || defined (_DEBUG))
#define MMCHK_VERBOSE
#endif

#include <string.h>
#include "stdio_f64.h"
#include "mmchk.h"

int main(int argc, char* argv[])
{
	mmchk_result mmr;

#ifdef MMCHK_VERBOSE
	char msg[65536];
	mmr.msg = (char*)msg;
#else
	mmr.msg = 0;
#endif
	if(argc < 2)
	{
		/*
		printf("args i got:\n");
		for(r =0; r < argc; ++r)
		{
		printf("%d:[%s]\n",r,argv[r]);
		}
		*/
		printf(
			"mmchk, the low complexity file completeness checker.\n"
			"usage:\n\tmmchk <file to check>\nsupport avi/matroska(mkv)/asf(wmv)/rm(rmvb)/iso(mdf,partial)/rar/mp4(mov) files for now\n"
			"output goes to stdout, use redirect.\n");
	}
	else if(argc == 2)
	{
		mmchk(argv[1], &mmr, 0);
#ifdef MMCHK_VERBOSE
		printf("file size actually: %lld bytes\nfile size expected: %lld bytes\nfile size estimation accuracy : %d\nerror level : %d\n%s",
			mmr.file_size,
			mmr.file_size_expected,
			mmr.file_size_estimation_accuracy,
			mmr.error_level,
			mmr.msg);
#else
		printf("file size actually: %lld bytes\nfile size expected: %lld bytes\nfile size estimation accuracy : %d\nerror level : %d\n",
			mmr.file_size,
			mmr.file_size_expected,
			mmr.file_size_estimation_accuracy,
			mmr.error_level);
#endif
	}
	else
	{
		int i;
		for(i = 1; i < argc; ++i)
		{
			mmchk(argv[i], &mmr, 0);
			printf("%d, %s\n", mmr.error_level, argv[i]);
		}
	}
#if WIN32
	system("PAUSE");
#endif
	return 0;
}

