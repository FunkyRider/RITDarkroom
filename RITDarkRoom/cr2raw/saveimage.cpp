#include "saveimage.h"
#include "os.h"

extern "C" {
#ifdef WIN32
#include "../libjpeg/jpeglib.h"
#else
#include "jpeglib.h"
#endif
}

ExifIfd* GenerateIfd0(uint32_t w, uint32_t h, uint16_t bpp, const ExifIfd* exif, bool hasStripData)
{
    ExifIfd* ifd0 = new ExifIfd();
    const uint32_t strip = w * bpp / 8;

    uint16_t t_bpp[] = {bpp / 3, bpp / 3, bpp / 3};
    int32_t t_resolution[] = {100, 1};
    String desc("Processed by RIT Darkroom 0.48b");
    String date((char*) exif->at(exif->find(0x9003))->data);

    ifd0->addTag(0x100)->setUInt32(w);                   // ImageWidth
    ifd0->addTag(0x101)->setUInt32(h);                   // ImageHeight
    ifd0->addTag(0x102)->set(EXIFTAG_USHORT, 3, &t_bpp); // BitsPerSample
    ifd0->addTag(0x103)->setUInt16(1);                   // Compression
    ifd0->addTag(0x106)->setUInt16(2);                   // Photometric
    ifd0->addTag(0x10E)->setString(desc);                // Description
    ifd0->addTag(exif->at(exif->find(0x10F))->clone());  // Make
    ifd0->addTag(exif->at(exif->find(0x110))->clone());  // Model
    if (hasStripData)
        ifd0->addTag(0x111)->setUInt32(0);               // StripOffsets
    ifd0->addTag(exif->at(exif->find(0x112))->clone());  // Model
    ifd0->addTag(0x115)->setUInt16(3);                   // SamplesPerPixel
    if (hasStripData)
        ifd0->addTag(0x116)->setUInt32(h);               // RowsPerStrip
    if (hasStripData)
        ifd0->addTag(0x117)->setUInt32(strip * h);       // StripByteCounts
    ifd0->addTag(0x11A)->setRational(t_resolution);      // xResolution
    ifd0->addTag(0x11B)->setRational(t_resolution);      // yResolution
    ifd0->addTag(0x128)->setUInt16(1);                   // ResolutionUnit
    ifd0->addTag(0x132)->setString(date);                // ModifyDate
    ifd0->addTag(0x8769)->setUInt32(0);                  // EXIF IFD

    return ifd0;
}

int SaveJPEG(void* ptr, uint32_t w, uint32_t h, uint32_t bpp, const char* file, int quality)
{
	FILE *fp = _fopen(file, "wb");
	if (fp == NULL)
		return -1;
	
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
 
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, fp);

	cinfo.image_width = w;
	cinfo.image_height = h;
	cinfo.input_components = 3;
	cinfo.in_color_space   = JCS_RGB;
	
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality (&cinfo, quality, true);
	jpeg_start_compress(&cinfo, true);
	
	JSAMPROW row_pointer;          /* pointer to a single row */
	
	char* cptr = (char*) ptr;
 
	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer = (JSAMPROW) cptr + cinfo.next_scanline * (bpp >> 3) * w;
		jpeg_write_scanlines(&cinfo, &row_pointer, 1);
	}
	jpeg_finish_compress(&cinfo);
	
	fclose(fp);
	
	return 0;
}

int SaveJPEG2(void* ptr, uint32_t w, uint32_t h, uint32_t bpp, const char* file, int quality, const ExifIfd* exif)
{
	FILE *fp = _fopen(file, "wb");
	if (fp == NULL)
		return -1;
	
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
 
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, fp);

	cinfo.image_width = w;
	cinfo.image_height = h;
	cinfo.input_components = 3;
	cinfo.in_color_space   = JCS_RGB;
	
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality (&cinfo, quality, true);
	jpeg_start_compress(&cinfo, true);

    {
        const uint8_t head[] = {'E', 'x', 'i', 'f', 0, 0, 'I', 'I', 0x2A, 0, 8, 0, 0, 0};    
        ExifIfd* ifd0 = GenerateIfd0(w, h, bpp, exif, false);

        // Serialize IFD0 and EXIF blocks to byte array for saving to disk
        size_t ifd0_len = ifd0->getSize();
	    size_t exif_len = exif->getSize();
        unsigned char* ifd0_buf = (unsigned char*) malloc(14 + ifd0_len + exif_len);
        ifd0->at(ifd0->find(0x8769))->setUInt32(8 + ifd0_len);
        memcpy(ifd0_buf, head, 14);
        ifd0->save(ifd0_buf + 14, 8);
	    exif->save(ifd0_buf + 14 + ifd0_len, 8 + ifd0_len);
        jpeg_write_marker(&cinfo, JPEG_APP0 + 1, ifd0_buf, 14 + ifd0_len + exif_len);
    }

	JSAMPROW row_pointer;          /* pointer to a single row */
	
	char* cptr = (char*) ptr;
 
	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer = (JSAMPROW) cptr + cinfo.next_scanline * (bpp >> 3) * w;
		jpeg_write_scanlines(&cinfo, &row_pointer, 1);
	}
	jpeg_finish_compress(&cinfo);
	
	fclose(fp);
	
	return 0;
}


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
    
    FILE* fp = _fopen(file, "rb");
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
	0x2A,0,	    	    	    	    	/*  02 magic number */
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

    FILE* fp = _fopen(file, "wb");
    fwrite(head, 1, sizeof (head), fp);
	
	char* cptr = (char*) ptr;
    for (uint32_t i = 0; i < h; i ++)
    {
        fwrite(cptr, 1, strip, fp);
        cptr += strip;
    }

    fclose(fp);
}

void SaveTIFF2(void* ptr, uint32_t w, uint32_t h, uint32_t bpp, const char* file, const ExifIfd* exif)
{
    const uint8_t head[] = {'I', 'I', 0x2A, 0, 8, 0, 0, 0};    
    const uint32_t strip = w * bpp / 8;
    ExifIfd* ifd0 = GenerateIfd0(w, h, bpp, exif, true);

    // Serialize IFD0 and EXIF blocks to byte array for saving to disk
    size_t ifd0_len = ifd0->getSize();
	size_t exif_len = exif->getSize();
    void* ifd0_buf = malloc(ifd0_len);
	void* exif_buf = malloc(exif_len);
    ifd0->at(ifd0->find(0x8769))->setUInt32(8 + ifd0_len);
    ifd0->at(ifd0->find(0x111))->setUInt32(8 + ifd0_len + exif_len);	
    ifd0->save(ifd0_buf, 8);
	exif->save(exif_buf, 8 + ifd0_len);

    FILE* fp = _fopen(file, "wb");
    fwrite(head, 1, sizeof (head), fp);
    fwrite(ifd0_buf, 1, ifd0_len, fp);
    fwrite(exif_buf, 1, exif_len, fp);
    
    free(exif_buf);
    free(ifd0_buf);
	
    // Write image strip data
	char* cptr = (char*) ptr;
    for (uint32_t i = 0; i < h; i ++)
    {
        fwrite(cptr, 1, strip, fp);
        cptr += strip;
    }

    fclose(fp);
}
