#pragma once

#include <stdio.h>
#include <stdint.h>
#include "exif.h"

int SaveJPEG(void* ptr, uint32_t w, uint32_t h, uint32_t bpp, const char* file, int quality);
int SaveJPEG2(void* ptr, uint32_t w, uint32_t h, uint32_t bpp, const char* file, int quality, const ExifIfd* exif);

char* LoadTIFF(uint32_t &w, uint32_t &h, uint32_t &bpp, const char* file);
void SaveTIFF(void* ptr, uint32_t w, uint32_t h, uint32_t bpp, const char* file);
void SaveTIFF2(void* ptr, uint32_t w, uint32_t h, uint32_t bpp, const char* file, const ExifIfd* exif);

