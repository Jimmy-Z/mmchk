#ifndef MMCHK_H
#define MMCHK_H

#include "basetype.h"


//error messages and error level

#define ERRN_OK 0
#define ERRN_NO_FILE -1
#define ERRN_TOO_SMALL -2
#define ERRN_FILE_UNKNOWN -3
#define ERRN_NO_EBML_HEADSIGN -99
#define ERRN_NO_MKV -99
#define ERRN_NOSEGSIZE -3
#define ERRN_NO_ASF_HEADER_OBJ -99
#define ERRN_READ -4
#define ERRN_CORRUPT -4
#define ERRN_INCOMPLETE -5
#define ERRN_PAD -6

#define ERR_OK                      "file status: ok\n"
#define ERR_NO_FILE                 "file status: not found\n"
#define ERR_TOO_SMALL               "file status: too small\n"
#define ERR_FILE_UNKNOWN            "file status: not recognized\n"
#define ERR_NO_EBML_HEADSIGN        "file status: corrupt\nno EBML header found, not a valid matroska file.\n"
#define ERR_NO_MKV                  "file status: corrupt\nnot a valid matroska file.\n"
#define ERR_NO_ASF_HEADER_OBJ       "file status: corrupt\nnot a valid asf file.\n"
#define ERR_READ                    "file status: corrupt\nerror reading file.\n"
#define ERR_WRONG_CHUNK4CC          "file status: corrupt\nwrong chunk 4cc, corrupt file.\n"
#define ERR_WRONG_GUID              "file status: corrupt\nwrong chunk GUID, corrupt file.\n"
#define ERR_CORRUPT                 "file status: corrupt\ncorrupt data found while parsing file.\n"
#define ERR_INCOMPLETE              "file status: incomplete"//this must be treated separately
#define ERR_PAD                     "file status: oversize\nthere is padding at the end of file.\n"
#define ERR_NOT9660                 "file status: not a iso9660 compliant image.\n"
#define ERR_NOSEGSIZE               "file status: no segment size specified.\nmight be a recorded stream file.\n"

//file_size_estimation_accuracy
#define FSEA_PRECISELY 0
#define FSEA_SEGMENT_INCOMPLETE 2
#define FSEA_SEGMENT_COMPLETE 3
#define FSEA_AVI_CORRUPT 5
#define FSEA_NO_CLUE 0xff

typedef struct{
	UI64 file_size;
	UI64 file_size_expected;
	int file_size_estimation_accuracy;
	int error_level;
	char* msg;
}mmchk_result;

// main interface of mmchk
// parameters:
//    fn                IN, file name to check
//    mmr               IN, OUT, set mmr.msg to a valid buffer to receive detailed message
//                          or set it to null pointer to receive nothing
//    explicit_ext      IN, if you wanna specify a file type other than the extension in fn,
//                          use the explicit_ext parameter to pass the extension you want
//                          other wise just pass a null pointer

int mmchk(const char* fn, mmchk_result* mmr, const char * explicit_ext);

#endif

