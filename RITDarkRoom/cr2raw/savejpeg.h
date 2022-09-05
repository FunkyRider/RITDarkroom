#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

extern "C" {
#include "../libjpeg/jpeglib.h"
}

int SaveJPEG(void* ptr, uint32_t w, uint32_t h, uint32_t bpp, const char* file, int quality)
{
	FILE *fp = fopen(file, "wb");
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
