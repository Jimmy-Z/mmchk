/*
* mmchk.c Multimedia file checker
* author: JimmyZ
*/

#include <errno.h>
#include <string.h>
#include "stdio_f64.h"
#include "ebml.h"
#include "mmchk.h"

#if (defined (DEBUG) || defined (_DEBUG))
#define PRINT_CHUNK_ON 1
#define CHECK_TYPE 1
#include <assert.h>
#endif

#define RIFF_4CC 0x46464952 //"RIFF"
#define JUNK_4CC 0x4b4e554a //"JUNK"

//big endian so reverse order
#define EBML_Mark 0xa3df451a //Each EBML document has to start with this, see http://www.matroska.org/technical/specs/index.html
#define EBML_DocType 0x8242
#define Matroska_DocType 0x616b736f7274616dULL
#define Matroska_Segment 0x67805318

#pragma pack (push, 1)

typedef struct{
	UI32 FourCC;
	UI32 ChunkSize;
}AVI_ChunkHeader;

// RealMedia File Format according to http://wiki.multimedia.cx/index.php?title=RealMedia

typedef struct{
	UI32 FourCC;
	UI32 ChunkSize;
	UI16 ChunkVersion;
}RMF_ChunkHeader;

typedef struct{
	UI32 file_version;
	UI32 nHeaders;
}RMF_Header;

//asf file formate according to http://avifile.sourceforge.net/asf-1.0.htm
typedef struct{
	UI32 v1;
	UI16 v2;
	UI16 v3;
	UI8 v4[8];
}GUID;

typedef struct{
	GUID ChunkType;
	UI64 ChunkSize;
}ASF_ChunkHeader;

typedef struct{
	GUID ClientGUID;
	UI64 FileSize;
	UI64 FileTimeCreation;
	UI64 nPackets;
	UI64 TimeStampEnd;
	UI64 TimeDuration;
	UI32 TimeStampStart;
	UI32 Reserved;
	UI32 Flags;
	UI32 PacketSizeMin;
	UI32 PacketSizeMax;
	UI32 VideoFrameSize;
}ASF_HeaderObj;

//rar file format ,see TechNote.txt in WinRAR installation folder
typedef struct{
	UI16    HEAD_CRC;
	UI8     HEAD_TYPE;
	UI16    HEAD_FLAGS;
	UI16    HEAD_SIZE;
}RAR_HEADER;

#define HEAD_FLAG_ADDER_PRESENTS 0x8000
#define HIGH_PACK_SIZE 0x100

enum{
	HEAD_TYPE_MARKER_BLOCK          = 0x72,
	HEAD_TYPE_ARCHIVE_HEADER        = 0x73,
	HEAD_TYPE_FILE_HEADER           = 0x74,
	HEAD_TYPE_OLD_COMMENT_HEADER    = 0x75,
	HEAD_TYPE_OLD_AUTH_INFO         = 0x76,
	HEAD_TYPE_OLD_SUB_BLOCK         = 0x77,
	HEAD_TYPE_OLD_AUTH_INFO_ALT     = 0x79,
	HEAD_TYPE_SUBBLOCK              = 0x7a,
};
//there is always an unknown block with block type 0x7B at the end of an archive, ender block?

#pragma pack (pop)


static char msg_buffer[65536];

#define CAT_MSG(buffer) if(mmr->msg)strcat(mmr->msg, buffer)

//utility func

#if PRINT_CHUNK_ON
static void print_chunk(void * fcch)
{
	char fourCC[5];
	*(UI32*)fourCC = ((AVI_ChunkHeader*)fcch)->FourCC;
	fourCC[4] = 0;
	sprintf(msg_buffer, "found [%s] chunk : %d bytes\n",fourCC, ((AVI_ChunkHeader*)fcch)->ChunkSize + 8);
}
#endif

static void lowercase(char* p)
{
	while(*p)
	{
		if(*p >='A' && *p <= 'Z')
			*p = *p +'a'-'A';
		++p;
	}
}

static UI64 QWORD_BE2LE(UI64 b)
{
	UI64 a;
	((UI8*)&a)[0] = ((UI8*)&b)[7];
	((UI8*)&a)[1] = ((UI8*)&b)[6];
	((UI8*)&a)[2] = ((UI8*)&b)[5];
	((UI8*)&a)[3] = ((UI8*)&b)[4];
	((UI8*)&a)[4] = ((UI8*)&b)[3];
	((UI8*)&a)[5] = ((UI8*)&b)[2];
	((UI8*)&a)[6] = ((UI8*)&b)[1];
	((UI8*)&a)[7] = ((UI8*)&b)[0];
	return a;
}

static UI32 DWORD_BE2LE(UI32 b)
{
	UI32 a;
	((UI8*)&a)[0] = ((UI8*)&b)[3];
	((UI8*)&a)[1] = ((UI8*)&b)[2];
	((UI8*)&a)[2] = ((UI8*)&b)[1];
	((UI8*)&a)[3] = ((UI8*)&b)[0];
	return a;
}

static UI16 WORD_BE2LE(UI16 b)
{
	UI16 a;
	((UI8*)&a)[0] = ((UI8*)&b)[1];
	((UI8*)&a)[1] = ((UI8*)&b)[0];
	return a;
}

static int return_mmr(mmchk_result* mmr, int error_level, char* error_msg, int file_size_estimation_accuracy, UI64 file_size_expected)
{
	mmr->file_size_expected = file_size_expected;
	mmr->file_size_estimation_accuracy = file_size_estimation_accuracy;
	if(mmr->msg && error_msg)
		strcat(mmr->msg, error_msg);
	return mmr->error_level = error_level;
}

// check riff files, such as avi

int chkriff(FILE* f, mmchk_result* mmr)
{
	UI64 cur_size;
	fseek(f,0,SEEK_END);
	mmr->file_size = ftell(f);
	if(mmr->file_size < sizeof(AVI_ChunkHeader))
		return return_mmr(mmr, ERRN_TOO_SMALL, ERR_TOO_SMALL, FSEA_NO_CLUE, 0);

	cur_size = 0;
	while(cur_size + sizeof(AVI_ChunkHeader) < mmr->file_size )
	{
		AVI_ChunkHeader rh;
		fseek(f,cur_size,SEEK_SET);
		fread(&rh, sizeof(AVI_ChunkHeader), 1, f);
#if PRINT_CHUNK_ON
		print_chunk(&rh);
		CAT_MSG(msg_buffer);
#endif
#if 1 //debug codes, strange FourCC found
		if((rh.FourCC != RIFF_4CC)&&(rh.FourCC != JUNK_4CC))
			return return_mmr(mmr, ERRN_CORRUPT, ERR_READ, FSEA_AVI_CORRUPT, cur_size);
#else
		if((rh.FourCC != RIFF_4CC)&&(rh.FourCC != JUNK_4CC)/*&&(rh.FourCC != 0x815b8625)*/)//illegal 4cc code, corrupt file
		{
			CAT_MSG("error illegal 4cc code.\n");
			//return return_mmr(mmr, ERRN_CORRUPT, ERR_READ, FSEA_AVI_CORRUPT, cur_size);
		}
#endif
		cur_size += rh.ChunkSize + sizeof(AVI_ChunkHeader);
	}

	if(cur_size > mmr->file_size)
		return return_mmr(mmr, ERRN_INCOMPLETE, ERR_INCOMPLETE, FSEA_SEGMENT_INCOMPLETE, cur_size);
	else
		return return_mmr(mmr, ERRN_OK, ERR_OK, FSEA_SEGMENT_COMPLETE, cur_size);
}

//check matroska file, such as mkv

int chkmatroska(FILE* f, mmchk_result* mmr)
{
	int count,isMatroska = 0;
	UI64 header, header_length, length, number;
	UI8 buffer[8],ElementName[8];

	fseek(f,0,SEEK_END);
	mmr->file_size = ftell(f);
	if(mmr->file_size < 32)
		return return_mmr(mmr, ERRN_TOO_SMALL, ERR_TOO_SMALL, FSEA_NO_CLUE, 0);

	fseek(f,0,SEEK_SET);
	do{
		count = fgetc(f);
	}while((count != 0x1A)&&(count != EOF));

	if(count == EOF)
	{
		return return_mmr(mmr, ERRN_NO_EBML_HEADSIGN, ERR_NO_EBML_HEADSIGN, FSEA_NO_CLUE, 0);
	}

	ElementName[0] = count;
	if(EBML_dump_data(f,ElementName) != 4)
	{
		return return_mmr(mmr, ERRN_NO_EBML_HEADSIGN, ERR_NO_EBML_HEADSIGN, FSEA_NO_CLUE, 0);
	}
	if(*(UI32*)ElementName != EBML_Mark)
	{
		return return_mmr(mmr, ERRN_NO_EBML_HEADSIGN, ERR_NO_EBML_HEADSIGN, FSEA_NO_CLUE, 0);
	}

	if(EBML_get_number(f, &number) == -1)
	{
		return return_mmr(mmr, ERRN_CORRUPT, ERR_CORRUPT, FSEA_NO_CLUE, 0);
	}
	header_length = (long)number;
	sprintf(msg_buffer,"found EBML header, size: %d bytes\n",header_length);
	CAT_MSG(msg_buffer);
	header = ftell(f);

	//check DocType
	do{//find DocType
		if((count = EBML_get_data(f, ElementName)) == -1)
		{
			return return_mmr(mmr, ERRN_CORRUPT, ERR_CORRUPT, FSEA_NO_CLUE, 0);
		}
		//printf("%02X\n",*(UI16*)ElementName);
		if((count == 2) && (*(UI16*)ElementName == EBML_DocType))
		{//got DocType!
			if(EBML_get_number(f,&number) == -1)
			{
				return return_mmr(mmr, ERRN_CORRUPT, ERR_CORRUPT, FSEA_NO_CLUE, 0);
			}
			length = (long)number;
			if(length != 8)
			{
				return return_mmr(mmr, ERRN_NO_MKV, ERR_NO_MKV, FSEA_NO_CLUE, 0); // error DocType
			}
			fread(buffer,8,1,f);
			if(*(UI64*)buffer != Matroska_DocType)
			{
				return return_mmr(mmr, ERRN_NO_MKV, ERR_NO_MKV, FSEA_NO_CLUE, 0);
			}
			isMatroska = 1;
			break;
		}
		else
		{//ignore none DocType Elements
			if(EBML_get_number(f,&number) == -1)
			{
				return return_mmr(mmr, ERRN_CORRUPT, ERR_CORRUPT, FSEA_NO_CLUE, 0);
			}
			length = number;
			fseek(f,length,SEEK_CUR);
		}
	}while(1);

	if(!isMatroska)
	{
		return return_mmr(mmr, ERRN_NO_MKV, ERR_NO_MKV, FSEA_NO_CLUE, 0);// DocType not found, so not matroska
	}
	else
	{
		sprintf(msg_buffer, "found DocType: matroska\n");
		CAT_MSG(msg_buffer);
	}

	fseek(f, header + header_length, SEEK_SET);
	if((count = EBML_get_data(f, ElementName)) == -1)
	{
		return return_mmr(mmr, ERRN_CORRUPT, ERR_CORRUPT, FSEA_NO_CLUE, 0);
	}
	if((count == 4) && (*(UI32*)ElementName == Matroska_Segment))
	{
		UI64 fse;
		if((count = EBML_get_number(f,&number)) == -1)
		{
			return return_mmr(mmr, ERRN_CORRUPT, ERR_CORRUPT, FSEA_NO_CLUE, 0);
		}
		sprintf(msg_buffer, "found Segment, size: %lld bytes\n",number);
		CAT_MSG(msg_buffer);
		if(number == 0xFFFFFFFFFFFFFFULL)
			return return_mmr(mmr, ERRN_NOSEGSIZE, ERR_NOSEGSIZE, FSEA_NO_CLUE, 0);
		fse = header + header_length + 4 + count + number;
		if(fse == mmr->file_size)
			return return_mmr(mmr, ERRN_OK, ERR_OK, FSEA_PRECISELY, fse);
		else if(fse > mmr->file_size)
			return return_mmr(mmr, ERRN_INCOMPLETE, ERR_INCOMPLETE, FSEA_PRECISELY, fse);
		else
			return return_mmr(mmr, ERRN_PAD, ERR_PAD, FSEA_PRECISELY, fse);
	}
	else
	{//segment not found, so not matroska
		return return_mmr(mmr, ERRN_NO_MKV, ERR_NO_MKV, FSEA_NO_CLUE, 0);
	}
}

//check RealMedia files, such as rm/rmvb

int chkrm(FILE *f, mmchk_result* mmr)
{
	UI64 cur_size = 0;
	unsigned int counter = 0;
	RMF_ChunkHeader ChunkHeader;
	RMF_Header RMHeader;
	UI8 buffer[8];

	fseek(f, 0, SEEK_END);
	mmr->file_size = ftell(f);
	if(mmr->file_size < 4096)
		return return_mmr(mmr, ERRN_TOO_SMALL, ERR_TOO_SMALL, FSEA_NO_CLUE, 0);

	fseek(f, 0, SEEK_SET);
	if(!fread(&ChunkHeader, sizeof(RMF_ChunkHeader), 1, f))
	{
		return return_mmr(mmr, ERRN_CORRUPT, ERR_CORRUPT, FSEA_NO_CLUE, 0);
	}
	if(ChunkHeader.FourCC != 0x464D522E)
	{
		return return_mmr(mmr, ERRN_CORRUPT, ERR_WRONG_CHUNK4CC, FSEA_NO_CLUE, 0);// ".RMF" Header not found, not a valid rm file
	}
	ChunkHeader.ChunkSize = DWORD_BE2LE(ChunkHeader.ChunkSize);
#if PRINT_CHUNK_ON
	print_chunk(&ChunkHeader);
	CAT_MSG(msg_buffer);
#endif
	cur_size += ChunkHeader.ChunkSize;

	if(ChunkHeader.ChunkSize == 0x10)
	{//RMF_Header
		if(!fread(&buffer, 2, 1, f))
		{
			return return_mmr(mmr, ERRN_CORRUPT, ERR_CORRUPT, FSEA_NO_CLUE, 0);
		}
		RMHeader.file_version = WORD_BE2LE(*(UI16*)buffer);
	}
	else if(ChunkHeader.ChunkSize == 0x12)
	{//RMF_Header2
		if(!fread(&buffer, 4, 1, f))
		{
			return return_mmr(mmr, ERRN_CORRUPT, ERR_CORRUPT, FSEA_NO_CLUE, 0);
		}
		RMHeader.file_version = DWORD_BE2LE(*(UI32*)buffer);
	}
	else
	{
		return return_mmr(mmr, ERRN_CORRUPT, ERR_CORRUPT, FSEA_NO_CLUE, 0);// unknown rm file header version
	}
	if(!fread(&buffer, 4, 1, f))
	{
		return return_mmr(mmr, ERRN_CORRUPT, ERR_CORRUPT, FSEA_NO_CLUE, 0);
	}
	RMHeader.nHeaders = DWORD_BE2LE(*(UI32*)buffer);

	sprintf(msg_buffer, "expecting %d chunks\n", RMHeader.nHeaders + 2);
	CAT_MSG(msg_buffer);

	do {
		fseek(f, cur_size, SEEK_SET);
		if(!fread(&ChunkHeader, sizeof(ChunkHeader), 1, f))
		{
			return return_mmr(mmr, ERRN_CORRUPT, ERR_CORRUPT, FSEA_NO_CLUE, 0);
		}
		ChunkHeader.ChunkSize = DWORD_BE2LE(ChunkHeader.ChunkSize);
#if PRINT_CHUNK_ON
		print_chunk(&ChunkHeader);
		CAT_MSG(msg_buffer);
#endif
		cur_size += ChunkHeader.ChunkSize;
		++counter;
	} while((cur_size < mmr->file_size) && (counter < RMHeader.nHeaders + 2) && ChunkHeader.ChunkSize);

	if(cur_size < mmr->file_size)
		return return_mmr(mmr, ERRN_PAD, ERR_PAD, FSEA_SEGMENT_COMPLETE, cur_size);// strange, pad is common
	if(cur_size > mmr->file_size)
		return return_mmr(mmr, ERRN_INCOMPLETE, ERR_INCOMPLETE, FSEA_SEGMENT_INCOMPLETE, cur_size);// incomplete rm file;
	else if(counter == RMHeader.nHeaders + 2)
		return return_mmr(mmr, ERRN_OK, ERR_OK, FSEA_PRECISELY, cur_size);
	else
		return return_mmr(mmr, ERRN_INCOMPLETE, ERR_INCOMPLETE, FSEA_SEGMENT_COMPLETE, cur_size);// segment complete, but number of segments didn't match;
}

//check M$ Advanced Stream Format files, such as asf/wmv

int chkasf(FILE* f, mmchk_result* mmr)
{
	UI32 nSubChunks, i;
	UI64 header_size;
	ASF_ChunkHeader asfh;
	ASF_HeaderObj asfho;

	static GUID GUID_file_header = {0x75B22630, 0x668E, 0x11CF, 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C};
	static GUID GUID_header_obj = {0x8CABDCA1, 0xA947, 0x11CF, 0x8E, 0xE4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65};

	fseek(f, 0, SEEK_END);
	mmr->file_size = ftell(f);
	if(mmr->file_size < sizeof(ASF_ChunkHeader))
		return return_mmr(mmr, ERRN_TOO_SMALL, ERR_TOO_SMALL, FSEA_NO_CLUE, 0);

	fseek(f, 0, SEEK_SET);
	fread(&asfh, sizeof(ASF_ChunkHeader), 1, f);
	header_size = (long)asfh.ChunkSize;
	if(memcmp(&asfh.ChunkType, &GUID_file_header, sizeof(GUID)))
		return return_mmr(mmr, ERRN_CORRUPT, ERR_WRONG_GUID, FSEA_NO_CLUE, 0);
	if(mmr->file_size < header_size)
		return return_mmr(mmr, ERRN_TOO_SMALL, ERR_TOO_SMALL, FSEA_NO_CLUE, 0);
	CAT_MSG("asf file header ok.\n");

	fread(&nSubChunks, 4, 1, f);
	fseek(f, 2, SEEK_CUR);

	for(i = 0; i < nSubChunks; ++i)
	{
		fread(&asfh, sizeof(ASF_ChunkHeader), 1, f);
		if(memcmp(&asfh.ChunkType, &GUID_header_obj, sizeof(GUID)))
		{
			fseek(f, (long)asfh.ChunkSize - sizeof(ASF_ChunkHeader), SEEK_CUR);
			if(ftell(f) > (long)header_size)
			{
				return return_mmr(mmr, ERRN_CORRUPT, ERR_CORRUPT, FSEA_NO_CLUE, 0);
			}
			continue;
		}
		else
		{//found GUID_header_obj
			sprintf(msg_buffer, "asf file header obj found at offset %d.\n", ftell(f));
			CAT_MSG(msg_buffer);
			fread(&asfho, sizeof(ASF_HeaderObj), 1, f);
			if(asfho.FileSize == mmr->file_size)
				return return_mmr(mmr, ERRN_OK, ERR_OK, FSEA_PRECISELY, (int)asfho.FileSize);
			else if(asfho.FileSize > mmr->file_size)
				return return_mmr(mmr, ERRN_INCOMPLETE, ERR_INCOMPLETE, FSEA_PRECISELY, (int)asfho.FileSize);
			else
				return return_mmr(mmr, ERRN_PAD, ERR_PAD, FSEA_PRECISELY, (int)asfho.FileSize);
		}
	}
	return return_mmr(mmr, ERRN_NO_ASF_HEADER_OBJ, ERR_NO_ASF_HEADER_OBJ, FSEA_NO_CLUE, 0);
}

//check iso9660 compliant iso files
//http://www.cinram.com/cd/tech/cdiso9660.pdf
//http://www.mactech.com/articles/develop/issue_03/high_sierra.html

int chkiso(FILE* f, mmchk_result *mmr)
{
	unsigned char CD001[5];
	int volume_space_size;
	UI64 expected_file_size = 0;

	fseek(f, 0, SEEK_END);
	mmr->file_size = ftell(f);
	if(mmr->file_size < 0x10000 + 4)
		return return_mmr(mmr, ERRN_TOO_SMALL, ERR_TOO_SMALL, FSEA_NO_CLUE, 0);

	fseek(f, 0x8000 + 1, SEEK_SET);
	fread(CD001, 5, 1, f);
#if 1 //check iso9660 signature
	if((CD001[0] != 'C')
		||(CD001[1] != 'D')
		||(CD001[2] != '0')
		||(CD001[3] != '0')
		||(CD001[4] != '1'))
	{
		fseek(f, 0x9310 - 0x8000 - 5, SEEK_CUR);//.mdf and some .img file has a iso9660 signature here
		fread(CD001, 5, 1, f);
		if((CD001[0] != 'C')
			||(CD001[1] != 'D')
			||(CD001[2] != '0')
			||(CD001[3] != '0')
			||(CD001[4] != '1'))
			return return_mmr(mmr, ERRN_FILE_UNKNOWN, ERR_NOT9660, FSEA_NO_CLUE, 0);
		expected_file_size += 0x9310 - 0x8000;
	}
#endif

	fseek(f, 0x8050 - 0x8000 - 1 - 5, SEEK_CUR);
	fread(&volume_space_size, 4, 1, f);

	expected_file_size += (long long)volume_space_size * 2048;

	if(expected_file_size == mmr->file_size)
		return return_mmr(mmr, ERRN_OK, ERR_OK, FSEA_PRECISELY, expected_file_size);
	else if(expected_file_size < mmr->file_size)
	{
		CAT_MSG("not a compliant iso9660 image?\n");
		return return_mmr(mmr, ERRN_PAD, ERR_PAD, FSEA_NO_CLUE, expected_file_size);
	}
	else
		return return_mmr(mmr, ERRN_INCOMPLETE, ERR_INCOMPLETE, FSEA_PRECISELY, expected_file_size);
}

// check rar archive files
int chkrar(FILE *f, mmchk_result *mmr)
{
	int ic;
	UI64 current = 0;
	RAR_HEADER header;
	unsigned char magic_word[6];
	int got_magic_word = 0;

	fseek(f, 0, SEEK_END);
	mmr->file_size = ftell(f);

	fseek(f, 0, SEEK_SET);
	while((ic = fgetc(f)) != EOF)
	{
		unsigned char c = ic;
		if(c == 'R')
		{
			current = ftell(f);
			if(current + 6 + 13 >= mmr->file_size)
			{
				return return_mmr(mmr, ERRN_FILE_UNKNOWN, ERR_FILE_UNKNOWN, FSEA_NO_CLUE, 0);
			}
			else
			{
				fread(magic_word, 6, 1, f);
				if((magic_word[0] == 'a')
					&&(magic_word[1] == 'r')
					&&(magic_word[2] == '!')
					&&(magic_word[3] == 0x1a)
					&&(magic_word[4] == 0x07)
					&&(magic_word[5] == 0x00))
				{
					got_magic_word = 1;
					sprintf(msg_buffer, "rar marker block with magic word [Rar!] found @ 0x%08X, size 7 bytes\n", current - 1);
					CAT_MSG(msg_buffer);
					break;
				}
			}
		}
	}

	if(!got_magic_word)
	{
		CAT_MSG("rar magic word [Rar!] not found!\n");
		return return_mmr(mmr, ERRN_FILE_UNKNOWN, ERR_FILE_UNKNOWN, FSEA_NO_CLUE, 0);
	}

	current = ftell(f);

	while (current + sizeof(RAR_HEADER) <= mmr->file_size)
	{
		unsigned int block_size = 0;
		unsigned int high_pack_size = 0;
		fseek(f, current, SEEK_SET);
		fread(&header, sizeof(RAR_HEADER), 1, f);
		if(header.HEAD_FLAGS & HEAD_FLAG_ADDER_PRESENTS)
		{
			if(current + sizeof(RAR_HEADER) + 4 <= mmr->file_size)
			{
				fread(&block_size, 4, 1, f);
				//debug codes
				//printf("adder size: %u\n", block_size);
			}
			else
			{
				return return_mmr(mmr, ERRN_INCOMPLETE, ERR_INCOMPLETE, FSEA_SEGMENT_INCOMPLETE, 0);
			}
		}
		block_size += header.HEAD_SIZE;
		switch(header.HEAD_TYPE)
		{
		case HEAD_TYPE_ARCHIVE_HEADER:
			CAT_MSG("rar archive header block found");
			break;
		case HEAD_TYPE_FILE_HEADER:
			CAT_MSG("rar file header block found");
			if(header.HEAD_FLAGS & HIGH_PACK_SIZE)
			{
				CAT_MSG("HIGH_PACK_SIZE presents\n");
				if(current + sizeof(RAR_HEADER) + 4 + 21 + 4> mmr->file_size)
					return return_mmr(mmr, ERRN_INCOMPLETE, ERR_INCOMPLETE, FSEA_SEGMENT_INCOMPLETE, 0);
				fseek(f, 21, SEEK_CUR);
				fread(&high_pack_size, 4, 1, f);
			}
			break;
		case HEAD_TYPE_OLD_COMMENT_HEADER:
			CAT_MSG("rar old style comment header block found");
			break;
		case HEAD_TYPE_OLD_AUTH_INFO:
			CAT_MSG("rar old style authenticity information block found");
			break;
		case HEAD_TYPE_OLD_SUB_BLOCK:
			CAT_MSG("rar old style subblock found");
			break;
		case HEAD_TYPE_OLD_AUTH_INFO_ALT:
			CAT_MSG("rar old style authenticity information block found");
			break;
		case HEAD_TYPE_SUBBLOCK:
			CAT_MSG("rar subblock found");
			break;
		default:
			sprintf(msg_buffer, "unknown rar block [%02X] found", (int)header.HEAD_TYPE);
			CAT_MSG(msg_buffer);
		}
		if(current < 0x100000000LL)
			sprintf(msg_buffer, " @ 0x%08llX, size %u bytes\n", current, block_size);
		else
			sprintf(msg_buffer, " @ 0x%016llX, size %u bytes\n", current, block_size);
		CAT_MSG(msg_buffer);
		if((high_pack_size == 0) && (block_size == 0))
		{
			sprintf(msg_buffer, "invalid block size: zero\n");
			CAT_MSG(msg_buffer);
			return return_mmr(mmr, ERRN_CORRUPT, ERR_CORRUPT, FSEA_NO_CLUE, 0);
		}
		current += ((UI64)high_pack_size << 32) + block_size;
	}

	if(current == mmr->file_size)
	{
		return return_mmr(mmr, ERRN_OK, ERR_OK, FSEA_SEGMENT_COMPLETE, current);
	}
	else
	{
		return return_mmr(mmr, ERRN_INCOMPLETE, ERR_INCOMPLETE, FSEA_SEGMENT_INCOMPLETE, (current < mmr->file_size) ? current + 7 : current);
	}
}

// check ISO Base Media File Format according to ISO/IEC 14496-14, mp4/3gpp etc.
int chkisom(FILE *f, mmchk_result *mmr)
{
	UI64 current = 0;
	UI32 box_size;
	UI64 box_size_large;
	UI64 box_size_final;
	char box_type[5];

	box_type[4] = '\0';

	fseek(f, 0, SEEK_END);
	mmr->file_size = ftell(f);

	fseek(f, 0, SEEK_SET);
	while(mmr->file_size >= current + 8)
	{
		fseek(f, current, SEEK_SET);
		fread(&box_size, 4, 1, f);
		fread(box_type, 4, 1, f);

		if(box_size == 0)
		{
			return return_mmr(mmr, ERRN_NOSEGSIZE, ERR_FILE_UNKNOWN, FSEA_NO_CLUE, 0);
		}
		else if(box_size == 0x01000000) // 1 in big endian
		{
			fread(&box_size_large, 8, 1, f);
			box_size_final = QWORD_BE2LE(box_size_large);
		}
		else
		{
			box_size_final = DWORD_BE2LE(box_size);
		}
		if(current < 0x100000000LL)
			sprintf(msg_buffer, "[%s] box found @ 0x%08llX, size %llu bytes\n", box_type, current, box_size_final);
		else
			sprintf(msg_buffer, "[%s] box found @ 0x%016llX, size %llu bytes\n", box_type, current, box_size_final);
		CAT_MSG(msg_buffer);

		current += box_size_final;
	}

	if(current > mmr->file_size)
	{
		return return_mmr(mmr, ERRN_INCOMPLETE, ERR_INCOMPLETE, FSEA_SEGMENT_INCOMPLETE, current);
	}
	else
		return return_mmr(mmr, ERRN_OK, ERR_OK, FSEA_SEGMENT_COMPLETE, current);
}

// the main wrapper

int mmchk(const char* fn, mmchk_result* mmr, const char* explicit_ext)
{
	int r;
	FILE* f = fopen(fn, "rb");

#if CHECK_TYPE
	assert(sizeof(UI8) == 1);
	assert(sizeof(UI16) == 2);
	assert(sizeof(UI32) == 4);
	assert(sizeof(UI64) == 8);
	assert(sizeof(RAR_HEADER) == 7);
#endif

	mmr->file_size = 0;
	mmr->file_size_expected = 0;
	mmr->file_size_estimation_accuracy = 0;
	mmr->error_level = 0;
	if(mmr->msg)
		mmr->msg[0] = 0;

	if(f == NULL)
	{
		sprintf(msg_buffer, "can't find file [%s], %s\n", fn, strerror(errno));
		CAT_MSG(msg_buffer);
		r = return_mmr(mmr, ERRN_NO_FILE, ERR_NO_FILE, FSEA_NO_CLUE, 0); //file not recognized
	}
	else
	{
		int length;
		const char * dot;
		UI32 ext;
		char *sext = (char*)&ext;
		length = (int)strlen(fn);
		if(!explicit_ext)
		{
			for(dot = fn + length - 1; dot > fn; --dot)
			{
				if(*dot == '.')break;
			}
			ext = *(UI32*)(dot + 1);
		}
		else
		{
			ext = *(UI32*)explicit_ext;
		}
		sext[3] = '\0';
		lowercase(sext);
		switch(ext)
		{
		case 0x00697661://avi
			r = chkriff(f, mmr);
			break;
		case 0x00766b6d://mkv
			r = chkmatroska(f, mmr);
			break;
		case 0x62766d72://rmvb
		case 0x00766d72://rmv
		case 0x00006d72://rm
			r = chkrm(f, mmr);
			break;
		case 0x00667361://asf
		case 0x00766d77://wmv
			r = chkasf(f, mmr);
			break;
		case 0x006f7369://iso
		case 0x0066646d://mdf
			r = chkiso(f, mmr);
			break;
		case 0x00726172://rar
			r = chkrar(f, mmr);
			break;
		case 0x00657865://exe
			CAT_MSG("treat as a rar sfx archive.\n");
			r = chkrar(f, mmr);
			break;
		case 0x0034706d://mp4
		case 0x00766f6d://mov
			r = chkisom(f, mmr);
			break;
		default:
			if((sext[0] == 'r') && (sext[1] >= '0') && (sext[1] <= '9') && (sext[2] >= '0') && (sext[2] <= '9'))
				r = chkrar(f, mmr);
			else
			{
				sprintf(msg_buffer, "unknown file ext [%s][0x%08x]\n", dot, ext);
				CAT_MSG(msg_buffer);
				r = return_mmr(mmr, ERRN_FILE_UNKNOWN, ERR_FILE_UNKNOWN, FSEA_NO_CLUE, 0); //file not recognized
			}
			break;
		}
		fclose(f);
	}

	if(mmr->error_level == ERRN_INCOMPLETE)
	{
		sprintf(msg_buffer,"[%d%%]\n",(int)((float)mmr->file_size * 100 / (float)mmr->file_size_expected));
		CAT_MSG(msg_buffer);
	}

	return r;
}

