#pragma once

#include <stdint.h>

enum eImageFormat
{
    eImageFormat_Gray = 0,
    eImageFormat_RGB = 1,
    eImageFormat_YCbCr = 2,
    eImageFormat_RGBG = 3
};

enum eImageRotation
{
    eImageRotation_0,
    eImageRotation_90,
    eImageRotation_180,
    eImageRotation_270
};

enum eImageDemosaic
{
	eImageDemosaic_None = 0,
	eImageDemosaic_Nearest = 1,
	eImageDemosaic_Linear = 2,
	eImageDemosaic_AHD = 3
};

class Image
{
public:
    Image(int32_t w, int32_t h, eImageFormat fmt);
    ~Image();

    // Mutators
    float*      	channel(int32_t id);
    float*          swap(int32_t id, float* buffer);
    int32_t     	width() const;
    int32_t     	height() const;
    eImageFormat	format() const;

    // Conversions
    void fromRGB8(uint8_t* ptr);
    void fromRGB16(uint16_t* ptr);
    void toRGB8(uint8_t* ptr);
    void toRGB16(uint16_t* ptr);
    void toFormat(const eImageFormat fmt);

    void fromRAW16(uint16_t* ptr, int32_t sat, int32_t black, int32_t rx = 0, int32_t ry = 0, int32_t rw = 0);
    void toRAW16(uint16_t* ptr, int32_t sat, int32_t black, int32_t rx = 0, int32_t ry = 0, int32_t rw = 0);
    
    Image* demosaicRAW(const eImageDemosaic type, const float* cam_rgb = 0, const float* wb = 0) const;
    Image* remosaicRAW() const;
    
    void transform(const float* cm);
    void scale(const float* s);

    // Rotate / scale / crop
    Image* rotate(const eImageRotation r) const;
    Image* scale(int32_t w, int32_t h) const;
    Image* crop(int32_t x, int32_t y, int32_t w, int32_t h) const;
    
    void blit(Image* dest, int32_t ch_d, int32_t ch_s, int32_t x, int32_t y, int32_t w, int32_t h, int32_t dx, int32_t dy) const;

private:
    void _resolveAHD(int32_t dx, int32_t dy, int32_t dw, int32_t dh, float* g, float** buf, int8_t** homo, const float* mat, const float* wb);

    float*          _ch[4];
    int32_t         _nch;
    eImageFormat    _format;
    int32_t         _size;
    int32_t         _w, _h;
};


void m3x3Multiply(float* result, const float* A, const float* B);
float m3x3Determinant(const float* A);
void m3x3Normalize(float* result, const float* A);
void m3x3Inverse(float* result, const float* A);
void m3x3Interpolate(float* result, const float* A, const float* B, float s);

int omp_get_num_threads();
int omp_get_thread_num();