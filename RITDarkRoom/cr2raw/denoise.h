#include <stdint.h>
#include "image.h"

#ifndef __DENOISE__
#define __DENOISE__

enum DENOISE_MODE
{
    DENOISE_LUMA = 0,
    DENOISE_CHROMA = 1,
    DENOISE_LINEAR = 2
};

uint32_t Denoise_Impulse(uint8_t* dst, uint8_t* src, const int32_t w, const int32_t h, int32_t threshold);
void Denoise_Wavelet(void* src, uint32_t w, const uint32_t h, float yamt, float ylow, float cbcramt, float cbcrlow, float contrast);

Image* Denoise_Impulse(Image* img, int c, const float threshold);
Image* Denoise_Wavelet(Image* img, float yamt, float ylow, float cbcramt, float cbcrlow, float contrast);

void wavelet_denoise(float *fimg[3], const uint32_t width, const uint32_t height, const float threshold, const float low, const DENOISE_MODE mode, const char* ch, const float contrast);

#endif