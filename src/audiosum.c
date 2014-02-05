/******************************************************************************

audiosum.c -- Prints the hash of an audio file excluding tag sections.
(C) Copyright 2001-2008 Octavio Alvarez Piza, alvarezp@alvarezp.ods.org

Released under GNU/GPL license. All other rights reserved.

## For program operation, see the showhelp() function. ##

Version history before Git:

 + Code has been restructured to avoid nested if().
 + Code now uses MD5 instead of CRC32.
 + Moved to Linux, so development is now focused on Linux. Sorry, Win32 folks.
 + Added "--help" switch.
 + 0.1.1: + Now knows how to ignore Lyrics3 sections. Since I
   don't have MP3 files with Lyrics3v1, this feature
   is not tested, and may not work as expected.
 + 0.1.0: + Initial Release. Plain program, knows how to ignore
   ID3v1.x and ID3v2.x tags. Never released.

******************************************************************************/

#include <stdio.h>
#include <math.h>
#include <string.h>

#include <mhash.h>

#include "config.h"

/* #define DEBUG */

/* This code will most probably go away. */
#ifdef DEBUG
void printd(char *msg, const char *filename, int code)
{
	fprintf(stderr, "%s:%04X:%s\n", msg, code, filename);
}
#else							/* */
#define printd(a,b,c) ((void)0)
#endif							/* */

hashid default_algorithm = MHASH_MD5;

struct algorithms {
	const char *cmdline_name;
	hashid libmhash_id;
} algorithms[] = {
//	{ "crc32",     MHASH_CRC32     },
//	{ "crc32b",    MHASH_CRC32B    },
//	{ "adler32",   MHASH_ADLER32   },
//	{ "md2",       MHASH_MD2       },
//	{ "md4",       MHASH_MD4       },
	{ "md5",       MHASH_MD5       },
	{ "sha1",      MHASH_SHA1      },
	{ "sha224",    MHASH_SHA224    },
	{ "sha256",    MHASH_SHA256    },
	{ "sha384",    MHASH_SHA384    },
	{ "sha512",    MHASH_SHA512    },
	{ "gost",      MHASH_GOST      },
	{ "ripemd128", MHASH_RIPEMD128 },
	{ "ripemd160", MHASH_RIPEMD160 },
	{ "ripemd256", MHASH_RIPEMD256 },
	{ "ripemd320", MHASH_RIPEMD320 },
	{ "tiger128",  MHASH_TIGER128  },
	{ "tiger160",  MHASH_TIGER160  },
	{ "tiger192",  MHASH_TIGER192  },
	{ "haval224",  MHASH_HAVAL224  },
	{ "haval256",  MHASH_HAVAL256  },
	{ "haval192",  MHASH_HAVAL192  },
	{ "haval160",  MHASH_HAVAL160  },
	{ "haval128",  MHASH_HAVAL128  },
	{ "whirlpool", MHASH_WHIRLPOOL },
	{ "snefru128", MHASH_SNEFRU128 },
	{ "snefru256", MHASH_SNEFRU256 },
};

const int algorithms_n = sizeof(algorithms) / sizeof(algorithms[0]);

int
ProcessFileID3v1(FILE * f, unsigned long *OffsetStart,
				 unsigned long *OffsetEnd)
{
	char TagBuffer[3] = { 0 };

	if (fseek(f, *OffsetEnd - 128, SEEK_SET) != 0) {
		return -1;
	}

	if (fread(&TagBuffer, 3, 1, f) == 0) {
		return -1;
	}

	if (strncmp("TAG", TagBuffer, 3) != 0) {
		/* TAG not found. We are done. */
		return 0;
	}

	*OffsetEnd -= 128;
	return 1;
}

int
ProcessFileID3v2(FILE * f, unsigned long *OffsetStart,
				 unsigned long *OffsetEnd)
{
	int x;
	long id3size = 0;

	struct ID3V2_Header {
		unsigned char sign[3];
		unsigned char version[2];
		unsigned char flags;
		unsigned char size[4];
	} id3;

	fseek(f, *OffsetStart, SEEK_SET);

	if (fread(&id3, sizeof(struct ID3V2_Header), 1, f) <= 0) {
		return -1;
	}

	if (!((id3.sign[0] == 'I') &&
		(id3.sign[1] == 'D') &&
		(id3.sign[2] == '3') &&
		(id3.version[0] < 0xFF) &&
		(id3.size[0] < 0x80) &&
		(id3.size[1] < 0x80) &&
		(id3.size[2] < 0x80) && (id3.size[3] < 0x80)))
	{
		return 0;
	}

	for (x = 0; x < 4; x++)
		id3size += pow(128, x) * (unsigned long) (id3.size[3 - x] & 127);
	id3size += 10;
	if (id3.flags & 16)
		id3size += 10;
	*OffsetStart += id3size;

	return 1;

}

int
ProcessFileLyrics3v1(FILE * f, unsigned long *OffsetStart,
					 unsigned long *OffsetEnd)
{
	char L3Buffer[5109];
	int FoundTag = 0;

	if (fseek(f, *OffsetEnd - 9, SEEK_SET) != 0) {
		return -1;
	}

	if (fread(&L3Buffer, 9, 1, f) <= 0 ) {
		return -1;
	}

	if (strncmp("LYRICSEND", L3Buffer, 9) != 0) {
		/* We found no ending LYRICSEND tag. We are done. */
		return 0;
	}

	if (fseek(f, *OffsetEnd - 5109, SEEK_SET) != 0) {
		return -1;
	}

	/* Since maximum lyrics length is 5100, we go back 5109 and _search_
	 * from this point on. 5109 == 5100 + strlen("LYRICSEND")
	 *
	 * This is the recommended method from http://www.id3.org/Lyrics3
	 */
	if (fread(&L3Buffer, 1, 5109, f) <= 0) {
		return -1;
	}

	char *pos=strstr(L3Buffer, "LYRICSBEGIN");

	if (pos == 0) {
		/* We found LYRICSEND but not LYRICSBEGIN. Treat as unexpected. */
		return -1;
	}

	/* We found a valid LYRICSBEGIN signature, so we have found a complete
	 * record.
	 */
	*OffsetEnd -= 5109;
	*OffsetEnd += pos - (char *)&L3Buffer;

}

int
ProcessFileLyrics3v2(FILE * f, unsigned long *OffsetStart,
					 unsigned long *OffsetEnd)
{
	char L3Buffer[11];
	int FoundTag = 0;
	unsigned long size = 0;

	if (fseek(f, *OffsetEnd - 9, SEEK_SET) != 0) {
		return -1;
	}

	if (fread(&L3Buffer, 9, 1, f) <= 0) {
		return -1;
	}

	if (strncmp("LYRICS200", L3Buffer, 9) != 0) {
		/* We found no ending "LYRICS200" tag. We are done. */
		return 0;
	}

	if (fseek(f, *OffsetEnd - 15, SEEK_SET) != 0) {
		return -1;
	}

	if (fgets((char *) &L3Buffer, 7, f) <= 0) {
		return -1;
	}

	L3Buffer[6] = 0;
	sscanf(L3Buffer, "%lu", &size);

	int r = 0;
	if (fseek(f, *OffsetEnd - size - 15, SEEK_SET) != 0) {
		return -1;
	}

	if ((r = fread(&L3Buffer, 1, 11, f)) <= 0) {
		return -1;
	}

	if (!strncmp("LYRICSBEGIN", L3Buffer, 11)) {
		/* LYRICSBEGIN found where it should be. We are done. */
		*OffsetEnd -= size + 15;
		return 1;
	}

	/* LYRICSBEGIN not found where supposed to. Try as if the size mark
	 * included the ending signature.
	 */
	if (fseek(f, *OffsetEnd - size, SEEK_SET) != 0) {
		return -1;
	}

	if (fread(&L3Buffer, 11, 1, f) <= 0) {
		return -1;
	}

	if (!strncmp("LYRICSBEGIN", L3Buffer, 11)) {
		/* Yes, the size mark was invalid: it included the last ending
		 * signature.
		 *
		 * From the Lyrics3v2 spec: The size value includes the "LYRICSBEGIN"
		 * string, but does not include the 6 character size descriptor and
		 * the trailing "LYRICS200" string.
		 */
		*OffsetEnd -= size;
		return 2;
	}

	/* So the size mark is invalid. We didn't find the LYRICSBEGIN signature.
	 * Treat as an unexpected error.
	 */
	return -1;
}

unsigned long filesize(FILE * stream)
{
	long curpos, length;
	curpos = ftell(stream);
	fseek(stream, 0L, SEEK_END);
	length = ftell(stream);
	fseek(stream, curpos, SEEK_SET);
	return length;
}

void showhelp()
{
	const char *help = "\r\n\
usage: audiosum [options]\r\n\
\r\n\
Options:\r\n\
    -a algo    Choose a different algorithm from MD5 for hashing.\r\n\
    -l         Print the list of supported hashes.\r\n\
    -b n (%)   Brief: Only compute n percent of the size of each file.\r\n\
               If n == 0 or ommited, only print the file size.\r\n\
    -h         Shows this help.\r\n\
\r\n\
Program operation:\r\n\
 + It reads a sequence of file names from stdin (they should be MP3 files)\r\n\
   sends to stdout the following information about them:\r\n\
    : File size, in hex format (8 chars).\r\n\
    : Hash of the file without ID3 or Lyrics tags, in hex format.\r\n\
    : What signatures were found.\r\n\
    : Complete file name.\r\n\
\r\n\
It tries to ignore non-audio parts. Currently ignored sections are:\r\n\
    : ID3v1.x\r\n\
    : ID3v2.x\r\n\
    : Lyrics3 v1 (not tested)\r\n\
    : Lyrics3 v2.00\r\n\
\r\n\
Usage:\r\n\
    : Audiosum is designed to be wrapped by sort and uniq, like this:\r\n\
      (you can copy and paste):\r\n\
\r\n\
find $HOME /mnt/music -iname \"*.mp3\" | \\\r\n\
    audiosum -b | sort | uniq -D -w 8 | cut -d ' ' -f 6- | \\\r\n\
    audiosum -b 2 | sort | uniq -D -w 41 | cut -d ' ' -f 7- | \\\r\n\
    audiosum | sort | uniq --all-repeated=separate -w 41 > result.txt\r\n\
\r\n\
";

	printf("%s", help);
}

void showhashes()
{
	hashid i = 0;

	printf("Supported hashes:\n\n");
	printf("    %-10s %4s\n", "Hash", "Bits");
	for (i = 0; i < algorithms_n; i++) {
		printf(" : %c%-10s %4d\n",
			algorithms[i].libmhash_id == default_algorithm? '*' : ' ',
			algorithms[i].cmdline_name,
			mhash_get_block_size(algorithms[i].libmhash_id)*8);
	}

	printf("\n * Default algorithm if -a is ommited\n\n");
}

int main(int arg_n, char *arg[])
{
	char filename[1024];

	int i;
	MHASH td;
	unsigned char buffer[8192];
	unsigned char *hash;

	hashid hash_algorithm = default_algorithm;
	int brief=100;
	int help=0;
	char c;
	int errflg=0;
	unsigned long crc;
	FILE *f;
	int r;

	while ((c = getopt(arg_n, arg, ":lb:a:h")) != -1) {
		switch(c) {
		case 'a':
			for (i = 0; i < algorithms_n; i++) {
				if (strcmp(optarg, algorithms[i].cmdline_name) == 0) {
					hash_algorithm = algorithms[i].libmhash_id;
					break;
				}
			}
			if (i == algorithms_n) {
				fprintf(stderr, "audiosum: unknown algorithm name: %s\n",
					optarg);
				return 1;
			}
			break;
		case 'b':
			brief=atoi(optarg);
			break;
		case 'h':
			help++;
			break;
		case 'l':
			showhashes();
			return 0;
			break;
		case ':':
			switch (optopt) {
			case 'b':
				brief = 0;
				break;
			case 'a':
				fprintf(stderr,	"audiosum: warning: unspecified hash algorithm."
					" Using the default.\n");
				break;
			}
			break;
		case '?':
			fprintf(stderr,	"audiosum: unrecognized option: -%c\n", optopt);
			exit(EXIT_FAILURE);
			break;
		}
	}
	if (errflg || help) {
		showhelp();
		if (help)
			exit(EXIT_SUCCESS);	
		else 
			exit(EXIT_FAILURE);
	}

	while (fgets(filename, 1024, stdin)) {
		if (filename[strlen(filename) - 1] == '\n') {
			filename[strlen(filename) - 1] = '\0';
		}

		f = fopen(filename, "rb");
		if (!f) {
			fprintf(stderr, "ERROR:Opening file: %s\n", filename);
			continue;
		}

		unsigned long OffsetStart = 0;
		unsigned long OffsetEnd = filesize(f);

		char *hadi3v1;
		if (!(r = ProcessFileID3v1(f, &OffsetStart, &OffsetEnd)) < 0) {
			printd("ERROR:Unexp, Analyzing ID3v1", filename, r);
			fclose(f);
			continue;
		}
		if (r == 0)
			hadi3v1 = "----";
		if (r == 1)
			hadi3v1 = "I3v1";

		char *hadi3v2;
		if (!(r = ProcessFileID3v2(f, &OffsetStart, &OffsetEnd)) < 0) {
			printd("ERROR:Unexp, Analyzing ID3v2", filename, r);
			fclose(f);
			continue;
		}
		if (r == 0)
			hadi3v2 = "----";
		if (r == 1)
			hadi3v2 = "I3v2";

		char *hadl3v1;
		if (!(r = ProcessFileLyrics3v1(f, &OffsetStart, &OffsetEnd)) < 0) {
			printd("ERROR:Unexp, Analyzing Lyrics3v1", filename, r);
			fclose(f);
			continue;
		}
		if (r == 0)
			hadl3v1 = "----";
		if (r == 1)
			hadl3v1 = "L3v1";

		char *hadl3v2;
		if (!(r = ProcessFileLyrics3v2(f, &OffsetStart, &OffsetEnd)) < 0) {
			printd("ERROR:Unexp, Analyzing Lyrics3v2", filename, r);
			fclose(f);
			continue;
		}
		if (r == 0)
			hadl3v2 = "----";
		if (r == 1)
			hadl3v2 = "L3v2";
		if (r == 2)
			hadl3v2 = "l3v2";

		fseek(f, OffsetStart, SEEK_SET);

		printf("%08lx ", OffsetEnd - OffsetStart);

		if (brief > 0) {
			td = mhash_init(hash_algorithm);

			if (td == MHASH_FAILED)
				exit(1);

			int howmany = ((OffsetEnd - OffsetStart + 1)*brief)/100;
			while (howmany > 0
				   && (r =
					   fread(&buffer, 1, howmany > 8192 ? 8192 : howmany,
							 f)) > 0) {
				mhash(td, &buffer, r);
				howmany -= 8192;
			}

			hash = mhash_end(td);

			for (i = 0; i < mhash_get_block_size(hash_algorithm); i++) {
				printf("%.2x", hash[i]);
			}
			free(hash);
			printf(" ");
		}

		printf("[%s] [%s] [%s] [%s] %s\n", hadi3v1, hadi3v2, hadl3v1, hadl3v2, filename);

		fclose (f);

	}

	return 0;
}
