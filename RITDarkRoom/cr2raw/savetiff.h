#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define OFST_IMAGE_WIDTH    	(0x12)
#define OFST_IMAGE_HEIGHT   	(0x1e)
#define OFST_ROWS_PER_STRIP 	(0x66)
#define OFST_STRIP_BYTE_COUNTS	(0x72)
#define OFST_BITS_PER_SAMPLE	(0x9e)
#define OFST_START_OF_IMAGE 	(0xb4)

char* LoadTIFF(uint32_t &w, uint32_t &h, uint32_t &bpp, const char* file)
{
    unsigned char head[] =
    {
    	/* signature */
	'I','I',    	    	    	    	/*  00 signature */
	42,0,	    	    	    	    	/*  02 magic number */
	8,0,0,0,	    	    	    	    /*  04 offset to 1st IFD */
	
	12, 0,	    	    	    	    	/*  08 number IFDs=12 */
    /* tag(2), type(2), count(4), ValOfst(4) */
    0,1,    4,0,     1,0,0,0,  0,0,0,0,     /*> 12 ImageWidth */
    1,1,    4,0,     1,0,0,0,  0,0,0,0,     /*> 1e ImageHeight */
    2,1,    3,0,     3,0,0,0,  0x9e,0,0,0,  /*  2a BitsPerSample */
    3,1,    3,0,     1,0,0,0,  1,0,0,0,     /*  36 Compression */
    6,1,    3,0,     1,0,0,0,  2,0,0,0,     /*  42 Photometric */
    17,1,   4,0,     1,0,0,0,  0xb4,0,0,0,  /*  4e StripOffsets */
    21,1,   3,0,     1,0,0,0,  3,0,0,0,     /*  5a SamplesPerPixel */
	22,1,   4,0,     1,0,0,0,  0,0,0,0,     /*> 66 RowsPerStrip */
	23,1,   4,0,     1,0,0,0,  0,0,0,0,     /*> 72 StripByteCounts*/
	26,1,   5,0,     1,0,0,0,  0xa4,0,0,0,  /*  7e xResolution*/
	27,1,   5,0,     1,0,0,0,  0xac,0,0,0,  /*  8a yResolution*/
	40,1,   3,0,     1,0,0,0,  1,0,0,0,     /*  96 ResolutionUnit*/
	0,0,0,0,     	    	    	    	/*  9a IFD terminator */	
	16,0,16,0,16,0,	    	    	    	/*  9e data for BitsPerSample */
	1,0,0,0,1,0,0,0,    	    	    	/*  a4 data for xResolution */
	1,0,0,0,1,0,0,0    	    	    	    /*  ac data for yResolution */
	    	    	    	    	    	/*  b4 start of image */
    };
    
    FILE* fp = fopen(file, "rb");
    if (fread(head, 1, sizeof (head), fp) < sizeof (head))
    	return 0;
    
    w = *((uint32_t *) (head + OFST_IMAGE_WIDTH));
    h = *((uint32_t *) (head + OFST_IMAGE_HEIGHT));
    uint32_t strip = *((uint32_t *) (head + OFST_STRIP_BYTE_COUNTS));
    
    char* ptr = (char*) malloc(w * h * 3);
    char* cptr = ptr;
    
    for (uint32_t i = 0; i < h; i ++)
    {
        uint32_t read = fread(cptr, 1, strip, fp);
        cptr += read;
    }

    fclose(fp);
    
    return ptr;
}

void SaveTIFF(void* ptr, uint32_t w, uint32_t h, uint32_t bpp, const char* file)
{
    unsigned char head[] =
    {
    	/* signature */
	'I','I',    	    	    	    	/*  00 signature */
	42,0,	    	    	    	    	/*  02 magic number */
	8,0,0,0,	    	    	    	/*  04 offset to 1st IFD */
	
	12, 0,	    	    	    	    	/*  08 number IFDs=12 */
     /* tag(2), type(2), count(4), ValOfst(4) */
    	0,1,    4,0,     1,0,0,0,  0,0,0,0,     /*> 12 ImageWidth */
    	1,1,    4,0,     1,0,0,0,  0,0,0,0,     /*> 1e ImageHeight */
    	2,1,    3,0,     3,0,0,0,  0x9e,0,0,0,  /*  2a BitsPerSample */
    	3,1,    3,0,     1,0,0,0,  1,0,0,0,     /*  36 Compression */
    	6,1,    3,0,     1,0,0,0,  2,0,0,0,     /*  42 Photometric */
    	17,1,   4,0,     1,0,0,0,  0xb4,0,0,0,  /*  4e StripOffsets */
    	21,1,   3,0,     1,0,0,0,  3,0,0,0,     /*  5a SamplesPerPixel */
	22,1,   4,0,     1,0,0,0,  0,0,0,0,     /*> 66 RowsPerStrip */
	23,1,   4,0,     1,0,0,0,  0,0,0,0,     /*> 72 StripByteCounts*/
	26,1,   5,0,     1,0,0,0,  0xa4,0,0,0,  /*  7e xResolution*/
	27,1,   5,0,     1,0,0,0,  0xac,0,0,0,  /*  8a yResolution*/
	40,1,   3,0,     1,0,0,0,  1,0,0,0,     /*  96 ResolutionUnit*/
	0,0,0,0,     	    	    	    	/*  9a IFD terminator */
	
	16,0,16,0,16,0,	    	    	    	/*  9e data for BitsPerSample */
	1,0,0,0,1,0,0,0,    	    	    	/*  a4 data for xResolution */
	1,0,0,0,1,0,0,0    	    	    	/*  ac data for yResolution */
	    	    	    	    	    	/*  b4 start of image */
    };

    if (bpp == 24)
    {
        head[OFST_BITS_PER_SAMPLE]     = 
	    head[OFST_BITS_PER_SAMPLE + 2] = 
	    head[OFST_BITS_PER_SAMPLE + 4] = bpp / 3;
    }

    uint32_t strip = w * bpp / 8;

    *((uint32_t *) (head + OFST_IMAGE_WIDTH)) = w;
    *((uint32_t *) (head + OFST_IMAGE_HEIGHT)) = h;
    *((uint32_t *) (head + OFST_ROWS_PER_STRIP)) = h;
    *((uint32_t *) (head + OFST_STRIP_BYTE_COUNTS)) = strip * h;

    FILE* fp = fopen(file, "wb+");
    fwrite(head, 1, sizeof (head), fp);
	
	char* cptr = (char*) ptr;
    for (uint32_t i = 0; i < h; i ++)
    {
        fwrite(cptr, 1, strip, fp);
        cptr += strip;
    }

    fclose(fp);
}


void SaveTIFF2(void* ptr, uint32_t w, uint32_t h, uint32_t bpp, const char* file, void* exif_blk, size_t exif_len)
{
    unsigned char head[] =
    {
    	/* signature */
	'I','I',    	    	    	    	/*  00 signature */
	42,0,	    	    	    	    	/*  02 magic number */
	8,0,0,0,	    	    	    	    /*  04 offset to 1st IFD */
	
	13, 0,	    	    	    	    	/*  08 number IFDs=13 */
     /* tag(2), type(2), count(4), ValOfst(4) */
    0,1,    4,0,     1,0,0,0,  0,0,0,0,     /*> 12 ImageWidth */
    1,1,    4,0,     1,0,0,0,  0,0,0,0,     /*> 1e ImageHeight */
    2,1,    3,0,     3,0,0,0,  0xaa,0,0,0,  /*  2a BitsPerSample */
    3,1,    3,0,     1,0,0,0,  1,0,0,0,     /*  36 Compression */
    6,1,    3,0,     1,0,0,0,  2,0,0,0,     /*  42 Photometric */
    17,1,   4,0,     1,0,0,0,  0xc0,0,0,0,  /*  4e StripOffsets */
    21,1,   3,0,     1,0,0,0,  3,0,0,0,     /*  5a SamplesPerPixel */
	22,1,   4,0,     1,0,0,0,  0,0,0,0,     /*> 66 RowsPerStrip */
	23,1,   4,0,     1,0,0,0,  0,0,0,0,     /*> 72 StripByteCounts*/
	26,1,   5,0,     1,0,0,0,  0xb0,0,0,0,  /*  7e xResolution*/
	27,1,   5,0,     1,0,0,0,  0xb8,0,0,0,  /*  8a yResolution*/
	40,1,   3,0,     1,0,0,0,  1,0,0,0,     /*  96 ResolutionUnit*/
    0x69,0x87, 4,0,  1,0,0,0,  0xc0,0,0,0,  /*  a2 EXIF IFD */
	0,0,0,0,     	    	    	    	/*  a6 IFD terminator */
	
	16,0,16,0,16,0,	    	    	    	/*  aa data for BitsPerSample */
	1,0,0,0,1,0,0,0,    	    	    	/*  b0 data for xResolution */
	1,0,0,0,1,0,0,0    	    	    	    /*  b8 data for yResolution */
	    	    	    	    	    	/*  c0 start of image */
    };

    if (bpp == 24)
    {
        head[0xb2]     = 
	    head[0xb2 + 2] = 
	    head[0xb2 + 4] = bpp / 3;
    }

    uint32_t strip = w * bpp / 8;

    *((uint32_t *) (head + OFST_IMAGE_WIDTH)) = w;
    *((uint32_t *) (head + OFST_IMAGE_HEIGHT)) = h;
    *((uint32_t *) (head + OFST_ROWS_PER_STRIP)) = h;
    *((uint32_t *) (head + OFST_STRIP_BYTE_COUNTS)) = strip * h;
    *((uint32_t *) (head + 0x4e)) = 0xc0 + exif_len;

    FILE* fp = fopen(file, "wb+");
    fwrite(head, 1, sizeof (head), fp);
    fwrite(exif_blk, 1, exif_len, fp);
	
	char* cptr = (char*) ptr;
    for (uint32_t i = 0; i < h; i ++)
    {
        fwrite(cptr, 1, strip, fp);
        cptr += strip;
    }

    fclose(fp);
}
