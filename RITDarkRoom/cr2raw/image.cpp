#include "image.h"
//#include <omp.h>
#include <immintrin.h>
#include <smmintrin.h>
#include <stdio.h>
#include <memory.h>
#include "cpuinfo.h"

static int32_t eImageFormat_CH[] = {1, 3, 3, 4};

Image::Image(int32_t w, int32_t h, eImageFormat fmt)
    : _w(w),
    _h(h)
{
    _size = w * h;
    _format = fmt;
    _nch = eImageFormat_CH[(int) fmt];
    _ch[0] = _ch[1] = _ch[2] = _ch[3] = NULL;

    for (int32_t i = 0; i < _nch; i ++)
        _ch[i] = new float[_size];
}

Image::~Image()
{
    for (int32_t i = 0; i < _nch; i ++)
        delete[] _ch[i];
}

float* Image::channel(int32_t id)
{
    return _ch[id];
}

float* Image::swap(int32_t id, float* buffer)
{
    float* old = _ch[id];
    _ch[id] = buffer;
    return old;
}


int32_t Image::width() const
{
    return _w;
}

int32_t Image::height() const
{
    return _h;
}

eImageFormat Image::format() const
{
    return _format;
}

void Image::fromRGB8(uint8_t* ptr)
{
	if (CPUID::hasSSE4_1())
	{
#pragma omp parallel
		{
#pragma omp for
        for (int32_t i = 0; i < _size; i ++)
        {
            const int32_t p = i * 3;
            const __m128 irgb = _mm_cvtpu8_ps(_mm_setr_pi8(ptr[p], ptr[p + 1], ptr[p + 2], 0, 0, 0, 0, 0));
            const __m128 rgb = _mm_mul_ps(irgb, _mm_set1_ps(0.003921569f));
            _ch[0][i] = _mm_cvtss_f32(_mm_shuffle_ps(rgb, rgb, _MM_SHUFFLE(3, 2, 1, 0)));
            _ch[1][i] = _mm_cvtss_f32(_mm_shuffle_ps(rgb, rgb, _MM_SHUFFLE(0, 3, 2, 1)));
            _ch[2][i] = _mm_cvtss_f32(_mm_shuffle_ps(rgb, rgb, _MM_SHUFFLE(1, 0, 3, 2)));
        }
        _mm_empty();
		}
	}
	else
	{
#pragma omp parallel for
        for (int32_t i = 0; i < _size; i ++)
        {
            const int32_t p = i * 3;
            _ch[0][i] = (float) ptr[p] * 0.003921569f;
            _ch[1][i] = (float) ptr[p + 1] * 0.003921569f;
            _ch[2][i] = (float) ptr[p + 2] * 0.003921569f;
        }
	}
}

void Image::fromRGB16(uint16_t *ptr)
{
	if (CPUID::hasSSE4_1())
	{
#pragma omp parallel
		{
#pragma omp for
        for (int32_t i = 0; i < _size; i ++)
        {
            const int32_t p = i * 3;
            const __m128 irgb = _mm_cvtpu16_ps(_mm_setr_pi16(ptr[p], ptr[p + 1], ptr[p + 2], 0));
            const __m128 rgb = _mm_mul_ps(irgb, _mm_set1_ps(1.5259e-5f));
            _ch[0][i] = _mm_cvtss_f32(_mm_shuffle_ps(rgb, rgb, _MM_SHUFFLE(3, 2, 1, 0)));
            _ch[1][i] = _mm_cvtss_f32(_mm_shuffle_ps(rgb, rgb, _MM_SHUFFLE(0, 3, 2, 1)));
            _ch[2][i] = _mm_cvtss_f32(_mm_shuffle_ps(rgb, rgb, _MM_SHUFFLE(1, 0, 3, 2)));
        }
        _mm_empty();
		}
	}
	else
	{
#pragma omp parallel for
        for (int32_t i = 0; i < _size; i ++)
        {
            const int32_t p = i * 3;
            _ch[0][i] = (float) ptr[p] * 1.5259e-5f;
            _ch[1][i] = (float) ptr[p + 1] * 1.5259e-5f;
            _ch[2][i] = (float) ptr[p + 2] * 1.5259e-5f;
        }
	}
}

void Image::toRGB8(uint8_t* ptr)
{
	if (CPUID::hasSSE4_1())
	{
#pragma omp parallel
		{
#pragma omp for
        for (int32_t i = 0; i < _size; i ++)
        {
            const int32_t p = i * 3;
            const __m128 rgb = _mm_min_ps(_mm_max_ps(_mm_setr_ps(_ch[0][i], _ch[1][i], _ch[2][i], 0.0f), _mm_set1_ps(0.0f)), _mm_set1_ps(1.0f));
            const __m128i irgb = _mm_cvtps_epi32(_mm_mul_ps(rgb, _mm_set1_ps(255.0f)));
            ptr[p] = (uint8_t) (_mm_extract_epi32(irgb, 0) & 0xFF);
            ptr[p + 1] = (uint8_t) (_mm_extract_epi32(irgb, 1) & 0xFF);
            ptr[p + 2] = (uint8_t) (_mm_extract_epi32(irgb, 2) & 0xFF);
        }
        _mm_empty();
		}
	}
	else
	{
#pragma omp parallel for
        for (int32_t i = 0; i < _size; i ++)
        {
            const int32_t p = i * 3;
            float f = _ch[0][i] * 255.0f;
            if (f < 0) f = 0; else if (f > 255.0f) f = 255.0f;
            ptr[p] = (uint8_t) f;
            f = _ch[1][i] * 255.0f;
            if (f < 0) f = 0; else if (f > 255.0f) f = 255.0f;
            ptr[p + 1] = (uint8_t) f;
            f = _ch[2][i] * 255.0f;
            if (f < 0) f = 0; else if (f > 255.0f) f = 255.0f;
            ptr[p + 2] = (uint8_t) f;
		}
	}
}

void Image::toRGB16(uint16_t *ptr)
{
	if (CPUID::hasSSE4_1())
	{
#pragma omp parallel
		{
#pragma omp for
        for (int32_t i = 0; i < _size; i ++)
        {
            const int32_t p = i * 3;
            const __m128 rgb = _mm_min_ps(_mm_max_ps(_mm_setr_ps(_ch[0][i], _ch[1][i], _ch[2][i], 0.0f), _mm_set1_ps(0.0f)), _mm_set1_ps(1.0f));
            const __m128i irgb = _mm_cvtps_epi32(_mm_mul_ps(rgb, _mm_set1_ps(65535.0f)));
            ptr[p] = (uint16_t) (_mm_extract_epi32(irgb, 0) & 0xFFFF);
            ptr[p + 1] = (uint16_t) (_mm_extract_epi32(irgb, 1) & 0xFFFF);
            ptr[p + 2] = (uint16_t) (_mm_extract_epi32(irgb, 2) & 0xFFFF);
        }
        _mm_empty();
		}
	}
	else
	{
#pragma omp parallel for
        for (int32_t i = 0; i < _size; i ++)
        {
            const int32_t p = i * 3;
            float f = _ch[0][i] * 65535.0f;
            if (f < 0) f = 0; else if (f > 65535.0f) f = 65535.0f;
            ptr[p] = (uint16_t) f;
            f = _ch[1][i] * 65535.0f;
            if (f < 0) f = 0; else if (f > 65535.0f) f = 65535.0f;
            ptr[p + 1] = (uint16_t) f;
            f = _ch[2][i] * 65535.0f;
            if (f < 0) f = 0; else if (f > 65535.0f) f = 65535.0f;
            ptr[p + 2] = (uint16_t) f;
        }
	}
}

void Image::fromRAW16(uint16_t* ptr, int32_t sat, int32_t black, int32_t rx, int32_t ry, int32_t rw)
{
	const float l = (float) (sat - black);
    // Convert from bayer R-G, G-B block to 4 channel R-G-B-G image
    float* ch[] = {_ch[0], _ch[1], _ch[2], _ch[3]};

    if (rw == 0) rw = _w * 2;

    for (int32_t y = 0; y < _h; y ++)
    {
        uint16_t* lp = ptr + (y * 2 + ry) * rw + rx;
        for (int32_t x = 0; x < _w; x ++)
        {
            int32_t d = *lp ++ - black;
            *ch[0] ++ = (float) d / l;
            d = *lp ++ - black;
            *ch[1] ++ = (float) d / l;
        }

        lp = ptr + (y * 2 + ry + 1) * rw + rx;
        for (int32_t x = 0; x < _w; x ++)
        {
            int32_t d = *lp ++ - black;
            *ch[3] ++ = (float) d / l;
            d = *lp ++ - black;
            *ch[2] ++ = (float) d / l;
        }
    }
}

void Image::toRAW16(uint16_t* ptr, int32_t sat, int32_t black, int32_t rx, int32_t ry, int32_t rw)
{
    const float l = (float) (sat - black);
    float* ch[] = {_ch[0], _ch[1], _ch[2], _ch[3]};

	float fsat = (float) sat;
    float fb = (float) black + 0.5f;
	float ffb = (float) black - 16.0f;
	if (ffb < 0) ffb = 0;

    if (rw == 0) rw = _w * 2;

    for (int32_t y = 0; y < _h; y ++)
    {
        uint16_t* lp = ptr + (y * 2 + ry) * rw + rx;
        for (int32_t x = 0; x < _w; x ++)
        {
        	float f = *ch[0] ++ * l + fb;
        	if (f < ffb) f = ffb; else if (f > fsat) f = fsat;
        	*lp ++ = (uint16_t) f;
        	
        	f = *ch[1] ++ * l + fb;
        	if (f < ffb) f = ffb; else if (f > fsat) f = fsat;
        	*lp ++ = (uint16_t) f;
        }

        lp = ptr + (y * 2 + ry + 1) * rw + rx;
        for (int32_t x = 0; x < _w; x ++)
        {
        	float f = *ch[3] ++ * l + fb;
        	if (f < ffb) f = ffb; else if (f > fsat) f = fsat;
        	*lp ++ = (uint16_t) f;
        	
        	f = *ch[2] ++ * l + fb;
        	if (f < ffb) f = ffb; else if (f > fsat) f = fsat;
        	*lp ++ = (uint16_t) f;
        }
    }
}

static const int AHD_TILE = 512;

Image* Image::demosaicRAW(const eImageDemosaic type, const float* cam_rgb, const float* wb) const
{
	int32_t dw = _w * 2, dh = _h * 2;
	Image* img = new Image(dw, dh, (type == eImageDemosaic_AHD) ? eImageFormat_RGBG : eImageFormat_RGB);
	
	if (type == eImageDemosaic_None)
	{
#pragma omp parallel for
		for (int32_t y = 0; y < _h; y ++)
    	{
    		const uint32_t slp = y * _w;
    		const uint32_t dlp = y * 2 * dw;
    		
        	for (int32_t x = 0; x < _w; x ++)
        	{
        		const uint32_t sp = slp + x;
				const uint32_t dp = dlp + x * 2;
				
        		const float r = *(_ch[0] + sp);
        		const float g1 = *(_ch[1] + sp);
        		const float g2 = *(_ch[3] + sp);
        		const float b = *(_ch[2] + sp);

        		*(img->_ch[0] + dp) = r;
        		*(img->_ch[1] + dp) = 0;	
        		*(img->_ch[2] + dp) = 0;
        		
        		*(img->_ch[0] + dp + 1) = 0;
        		*(img->_ch[1] + dp + 1) = g1;
        		*(img->_ch[2] + dp + 1) = 0;
     		
        		*(img->_ch[0] + dp + dw) = 0;
        		*(img->_ch[1] + dp + dw) = g2;        		
        		*(img->_ch[2] + dp + dw) = 0;

        		*(img->_ch[0] + dp + dw + 1) = 0;
        		*(img->_ch[1] + dp + dw + 1) = 0;
        		*(img->_ch[2] + dp + dw + 1) = b;
			}
		}	
	}
	else if (type == eImageDemosaic_Nearest)
	{
#pragma omp parallel for
		for (int32_t y = 0; y < _h; y ++)
    	{
    		const uint32_t slp = y * _w;
    		const uint32_t dlp = y * 2 * dw;
    		
        	for (int32_t x = 0; x < _w; x ++)
        	{
        		const uint32_t sp = slp + x;
				const uint32_t dp = dlp + x * 2;
				
        		const float r = *(_ch[0] + sp);
        		const float g1 = *(_ch[1] + sp);
        		const float g2 = *(_ch[3] + sp);
        		const float b = *(_ch[2] + sp);

        		*(img->_ch[0] + dp) = r;
        		*(img->_ch[1] + dp) = g1;        		
        		*(img->_ch[2] + dp) = b;
        		
        		*(img->_ch[0] + dp + 1) = r;
        		*(img->_ch[1] + dp + 1) = g1;        		
        		*(img->_ch[2] + dp + 1) = b;
     		
        		*(img->_ch[0] + dp + dw) = r;
        		*(img->_ch[1] + dp + dw) = g2;        		
        		*(img->_ch[2] + dp + dw) = b;

        		*(img->_ch[0] + dp + dw + 1) = r;
        		*(img->_ch[1] + dp + dw + 1) = g2;        		
        		*(img->_ch[2] + dp + dw + 1) = b;
			}
		}
	}
	else if (type == eImageDemosaic_Linear)
	{
#pragma omp parallel for
		for (int32_t y = 0; y < _h; y ++)
    	{
    		const uint32_t slp = y * _w;
    		const uint32_t dlp = y * 2 * dw;
    		
        	for (int32_t x = 0; x < _w; x ++)
        	{
        		const uint32_t sp = slp + x;
				const uint32_t dp = dlp + x * 2;	
        	
        		const float r = *(_ch[0] + sp);
        		const float g1 = *(_ch[1] + sp);
        		const float g2 = *(_ch[3] + sp);
        		const float b = *(_ch[2] + sp);
        		
        		const float l_g1 = (x == 0) ? r : *(_ch[1] + sp - 1);
        		const float l_b = (x == 0) ? b : *(_ch[2] + sp - 1);

        		const float r_r = (x == _w - 1) ? r : *(_ch[0] + sp + 1);
        		const float r_g2 = (x == _w - 1) ? r : *(_ch[3] + sp + 1);
        		
        		const float t_g2 = (y == 0) ? r : *(_ch[3] + sp - _w);
        		const float t_b = (y == 0) ? b : *(_ch[2] + sp - _w);

        		const float d_r = (y == _h - 1) ? r : *(_ch[0] + sp + _w);
        		const float d_g1 = (y == _h - 1) ? r : *(_ch[1] + sp + _w);
        		
        		const float tl_b = (x == 0) ? ((y == 0) ? b : t_b)
        							: ((y == 0) ? l_b : *(_ch[2] + sp - _w - 1));
        		const float dr_r = (x == _w - 1) ? ((y == _h - 1) ? r : d_r)
        							: ((y == _h - 1) ? r_r : *(_ch[0] + sp + _w + 1));
        		
        		*(img->_ch[0] + dp) = r;
        		*(img->_ch[1] + dp) = (g1 + l_g1 + g2 + t_g2) * 0.25f;
        		*(img->_ch[2] + dp) = (b + t_b + l_b + tl_b) * 0.25f;
        		
        		*(img->_ch[0] + dp + 1) = (r_r + r) * 0.5f;
        		*(img->_ch[1] + dp + 1) = g1;
        		*(img->_ch[2] + dp + 1) = (t_b + b) * 0.5f;
     		
        		*(img->_ch[0] + dp + dw) = (d_r + r) * 0.5f;
        		*(img->_ch[1] + dp + dw) = g2;
        		*(img->_ch[2] + dp + dw) = (l_b + b) * 0.5f;

        		*(img->_ch[0] + dp + dw + 1) = (r_r + d_r + r + dr_r) * 0.25f;
        		*(img->_ch[1] + dp + dw + 1) = (g2 + r_g2 + g1 + d_g1) * 0.25f;
        		*(img->_ch[2] + dp + dw + 1) = b;
			}
		}
	}
	else if (type == eImageDemosaic_AHD)
	{
#pragma omp parallel for
		for (int32_t y = 0; y < _h; y ++)
    	{
    		const uint32_t slp = y * _w;
    		const uint32_t dlp = y * 2 * dw;
    		
        	for (int32_t x = 0; x < _w; x ++)
        	{
        		const uint32_t sp = slp + x;
				const uint32_t dp = dlp + x * 2;	
        	
        		const float r = *(_ch[0] + sp);
        		const float g1 = *(_ch[1] + sp);
        		const float g2 = *(_ch[3] + sp);
        		const float b = *(_ch[2] + sp);
        		
        		const float l_g1 = (x == 0) ? r : *(_ch[1] + sp - 1);
        		const float l_b = (x == 0) ? b : *(_ch[2] + sp - 1);

        		const float r_r = (x == _w - 1) ? r : *(_ch[0] + sp + 1);
        		const float r_g2 = (x == _w - 1) ? r : *(_ch[3] + sp + 1);
        		
        		const float t_g2 = (y == 0) ? r : *(_ch[3] + sp - _w);
        		const float t_b = (y == 0) ? b : *(_ch[2] + sp - _w);

        		const float d_r = (y == _h - 1) ? r : *(_ch[0] + sp + _w);
        		const float d_g1 = (y == _h - 1) ? r : *(_ch[1] + sp + _w);
        		
        		const float tl_b = (x == 0) ? ((y == 0) ? b : t_b)
        							: ((y == 0) ? l_b : *(_ch[2] + sp - _w - 1));
        		const float dr_r = (x == _w - 1) ? ((y == _h - 1) ? r : d_r)
        							: ((y == _h - 1) ? r_r : *(_ch[0] + sp + _w + 1));
        		
        		*(img->_ch[0] + dp) = r;
        		*(img->_ch[1] + dp) = (g2 + t_g2) * 0.5f;
        		*(img->_ch[3] + dp) = (g1 + l_g1) * 0.5f;
        		*(img->_ch[2] + dp) = (b + t_b + l_b + tl_b) * 0.25f;
        		
        		*(img->_ch[0] + dp + 1) = (r_r + r) * 0.5f;
        		*(img->_ch[1] + dp + 1) = g1;
        		*(img->_ch[3] + dp + 1) = g1;
        		*(img->_ch[2] + dp + 1) = (t_b + b) * 0.5f;
     		
        		*(img->_ch[0] + dp + dw) = (d_r + r) * 0.5f;
        		*(img->_ch[1] + dp + dw) = g2;
        		*(img->_ch[3] + dp + dw) = g2;
        		*(img->_ch[2] + dp + dw) = (l_b + b) * 0.5f;

        		*(img->_ch[0] + dp + dw + 1) = (r_r + d_r + r + dr_r) * 0.25f;
        		*(img->_ch[1] + dp + dw + 1) = (g1 + d_g1) * 0.5f;
        		*(img->_ch[3] + dp + dw + 1) = (g2 + r_g2) * 0.5f;
        		*(img->_ch[2] + dp + dw + 1) = b;
        		
        		// G1 channel: horizontal interpolation
        		// G2 channel: vertical interpolation
			}
		}

        const float rgb_ycc[] = {0.299f, -0.168736f, 0.5f, 0.587f, -0.331264f, -0.418688f, 0.114f, 0.5f, -0.081312f};
	    float cam_ycc[9];
	    m3x3Multiply(cam_ycc, rgb_ycc, cam_rgb);

        float* g = new float[_w * _h * 4];
        memset(g, 0, _w * _h * 4 * sizeof (float));

#pragma omp parallel
        {
	        float* buf[6];
	        for (int c = 0; c < 6; c ++) buf[c] = new float[AHD_TILE * AHD_TILE];
	        int8_t* homo[2];
	        for (int c = 0; c < 2; c ++) homo[c] = new int8_t[AHD_TILE * AHD_TILE];

#pragma omp for
            for (int y = 0; y < img->_h; y += AHD_TILE - 4)
            {
                int ddh = dh - y;
                if (ddh > AHD_TILE) ddh = AHD_TILE;
                for (int x = 0; x < img->_w * 2; x += AHD_TILE - 4)
                {
                    int ddw = dw - x;                
                    if (ddw > AHD_TILE) ddw = AHD_TILE;
                    img->_resolveAHD(x, y, ddw, ddh, g, buf, homo, cam_ycc, wb);
                }
            }

	        for (int c = 0; c < 2; c ++) delete[] homo[c];
	        for (int c = 0; c < 6; c ++) delete[] buf[c];
        }

	    delete img->_ch[3];
        delete img->_ch[1];
        img->_ch[1] = g;
	    img->_nch = 3;
	}
	
	return img;
}
#define MAX(a, b)	(((a) > (b)) ? (a) : (b)) 
#define MIN(a, b)	(((a) < (b)) ? (a) : (b)) 
#define SQR(a)		((a) * (a))

// Converts demosaic-ed AHD RGBG to RGB
// build homogenity map using given cam-rgb matrix and white balance
// choose G from two interpolated results
void Image::_resolveAHD(int32_t dx, int32_t dy, int32_t dw, int32_t dh, float* g, float** buf, int8_t** homo, const float* mat, const float* wb)
{
	const int _dir[4] = {-1, 1, -AHD_TILE, AHD_TILE};

	// Convert to YCbCr
	for (int32_t y = 0; y < dh; y ++)
    {
        const int32_t dl = (y + dy) * _w;
        const int32_t pl = y * AHD_TILE;
        for (int32_t x = 0; x < dw; x ++)
        {
            const int32_t d = dl + x + dx;
            const int32_t p = pl + x;

            const float r = _ch[0][d] * wb[0];
            const float g = _ch[1][d];
            const float b = _ch[2][d] * wb[2];
		    const float g2 = _ch[3][d];

            buf[0][p] = (r * mat[0] + g * mat[3] + b * mat[6]);
            buf[1][p] = (r * mat[1] + g * mat[4] + b * mat[7]);
            buf[2][p] = (r * mat[2] + g * mat[5] + b * mat[8]);
            buf[3][p] = (r * mat[0] + g2 * mat[3] + b * mat[6]);
            buf[4][p] = (r * mat[1] + g2 * mat[4] + b * mat[7]);
            buf[5][p] = (r * mat[2] + g2 * mat[5] + b * mat[8]);
	    }
    }

	// Build homogeneity map
	for (int32_t y = 1; y < dh - 1; y ++)
	{
		const int32_t lp = y * AHD_TILE;
		for (int32_t x = 1; x < dw - 1; x ++)
		{
			const int p = lp + x;

			float ldiff[4], abdiff[4];
			float ldiff2[4], abdiff2[4];

			for (int i = 0; i < 4; i ++)
			{
				{
					ldiff[i] = buf[0][p] - buf[0][p + _dir[i]];
					if (ldiff[i] < 0) ldiff[i] = -ldiff[i];
					const float a = buf[1][p] - buf[1][p + _dir[i]], b = buf[2][p] - buf[2][p + _dir[i]];
					abdiff[i] = a * a + b * b;
				}
				{
					ldiff2[i] = buf[3][p] - buf[3][p + _dir[i]];
					if (ldiff2[i] < 0) ldiff2[i] = -ldiff2[i];
					const float a = buf[4][p] - buf[4][p + _dir[i]], b = buf[5][p] - buf[5][p + _dir[i]];
					abdiff2[i] = a * a + b * b;
				}
			}

			const float leps = MIN(MAX(ldiff2[0], ldiff2[1]), MAX(ldiff[2], ldiff[3]));
			const float abeps = MIN(MAX(abdiff2[0], abdiff2[1]), MAX(abdiff[2], abdiff[3]));

			homo[0][p] = 0;
			homo[1][p] = 0;

			for (int i = 0; i < 4; i ++)
			{
				if (ldiff[i] <= leps && abdiff[i] <= abeps) homo[0][p] ++;
				if (ldiff2[i] <= leps && abdiff2[i] <= abeps) homo[1][p] ++;
			}
		}
	}

	// Choose Green based on homogeneity map
	for (int32_t y = 2; y < dh - 2; y ++)
	{
        const int32_t ld = (y + dy) * _w;
		const int32_t lp = y * AHD_TILE;
		for (int32_t x = 2; x < dw - 2; x ++)
		{
            const int d = ld + x + dx;
			const int p = lp + x;
			const int h1 = homo[0][p - AHD_TILE - 1] + homo[0][p - AHD_TILE] + homo[0][p - AHD_TILE + 1] + 
				homo[0][p - 1] + homo[0][p] + homo[0][p + 1] +
				homo[0][p + AHD_TILE - 1] + homo[0][p + AHD_TILE] + homo[0][p + AHD_TILE + 1];
			const int h2 = homo[1][p - AHD_TILE - 1] + homo[1][p - AHD_TILE] + homo[1][p - AHD_TILE + 1] + 
				homo[1][p - 1] + homo[1][p] + homo[1][p + 1] +
				homo[1][p + AHD_TILE - 1] + homo[1][p + AHD_TILE] + homo[1][p + AHD_TILE + 1];

			if (h2 > h1)
			{
				g[d] = _ch[3][d];
			}
            else if (h2 < h1)
			{
				g[d] = _ch[1][d];
			}
			else
			{
				g[d] = (_ch[1][d] + _ch[3][d]) / 2;
			}
		}
	}
}

Image* Image::remosaicRAW() const
{
	int32_t dw = _w / 2, dh = _h / 2;
	Image* img = new Image(dw, dh, eImageFormat_RGBG);
	
	for (int32_t y = 0; y < _h; y += 2)
    {
		const uint32_t slp = y * _w;
		const uint32_t dlp = y / 2 * dw;
		
    	for (int32_t x = 0; x < _w; x += 2)
    	{
    		const uint32_t sp = slp + x;
			const uint32_t dp = dlp + x / 2;
			
		    const float r = *(_ch[0] + sp);
    		const float g1 = *(_ch[1] + sp + 1);
    		const float g2 = *(_ch[1] + sp + _w);
    		const float b = *(_ch[2] + sp + 1 + _w);
    		
    		*(img->_ch[0] + dp) = r;
    		*(img->_ch[1] + dp) = g1;
    		*(img->_ch[2] + dp) = b;
    		*(img->_ch[3] + dp) = g2;
    	}
    }
	
	return img;
}

void Image::transform(const float* cm)
{
	if (_nch == 3)
	{
		for (int32_t i = 0; i < _size; i ++)
		{
			const float r = _ch[0][i];
			const float g = _ch[1][i];
			const float b = _ch[2][i];    		
			
			_ch[0][i] = r * cm[0] + g * cm[1] + b * cm[2];
			_ch[1][i] = r * cm[3] + g * cm[4] + b * cm[5];
			_ch[2][i] = r * cm[6] + g * cm[7] + b * cm[8];
		}
	}
	else if (_nch == 4)
	{
		for (int32_t i = 0; i < _size; i ++)
		{
			const float r = _ch[0][i];
			const float g = _ch[1][i];
			const float b = _ch[2][i];		
			const float d = _ch[3][i];
			
			_ch[0][i] = r * cm[0] + g * cm[1] + b * cm[2] + d * cm[3];
			_ch[1][i] = r * cm[4] + g * cm[5] + b * cm[6] + d * cm[7];
			_ch[2][i] = r * cm[8] + g * cm[9] + b * cm[10] + d * cm[11];
			_ch[3][i] = r * cm[12] + g * cm[13] + b * cm[14] + d * cm[15];
		}
	}
}

void Image::scale(const float* s)
{
	for (int32_t i = 0; i < _nch; i ++)
	{
		const float _s = s[i];
		float *c = _ch[i];
		
		for (int32_t i = 0; i < _size; i ++)
		{
			c[i] *= _s;
		}
	}
}

void Image::toFormat(const eImageFormat fmt)
{
    static const float IMAGEFORMAT_MAT[][4][4] = 
    {
        {{0.299f, 0.587f, 0.114f, 0.0f}, {-0.168736f, -0.331264f, 0.5f, 0.0f}, {0.5f, -0.418688f, -0.081312f, 0.0f}, {0, 0, 0, 0}},
        {{1.0f, 0.0f, 1.402f, 0.0f}, {1.0f, -0.34414f, -0.71414f, 0.0f}, {1.0f, 1.772f, 0.0, 0.0f}, {0, 0, 0, 0}}
    };

    if (_format == fmt)
        return;

    int32_t mat_id = 0;

    if (fmt == eImageFormat_YCbCr)
    {
        mat_id = 0;
    }
    else if (fmt == eImageFormat_RGB)
    {
        mat_id = 1;
    }

	if (CPUID::hasSSE4_1())
	{
		const __m128 to0 = _mm_loadu_ps(IMAGEFORMAT_MAT[mat_id][0]);
		const __m128 to1 = _mm_loadu_ps(IMAGEFORMAT_MAT[mat_id][1]);
		const __m128 to2 = _mm_loadu_ps(IMAGEFORMAT_MAT[mat_id][2]);

#pragma omp parallel
		{
#pragma omp for
		for (int32_t i = 0; i < _size; i ++)
		{
			const __m128 src = _mm_setr_ps(_ch[0][i], _ch[1][i], _ch[2][i], 0.0f);
			_ch[0][i] = _mm_cvtss_f32(_mm_dp_ps(src, to0, 0x71));
			_ch[1][i] = _mm_cvtss_f32(_mm_dp_ps(src, to1, 0x71));
			_ch[2][i] = _mm_cvtss_f32(_mm_dp_ps(src, to2, 0x71));
		}
		_mm_empty();
		}
	}
	else
	{
#pragma omp parallel for 
		for (int32_t i = 0; i < _size; i ++)
		{
			const float r = _ch[0][i], g = _ch[1][i], b = _ch[2][i];
			_ch[0][i] = r * IMAGEFORMAT_MAT[mat_id][0][0] + g * IMAGEFORMAT_MAT[mat_id][0][1] + b * IMAGEFORMAT_MAT[mat_id][0][2];
			_ch[1][i] = r * IMAGEFORMAT_MAT[mat_id][1][0] + g * IMAGEFORMAT_MAT[mat_id][1][1] + b * IMAGEFORMAT_MAT[mat_id][1][2];
			_ch[2][i] = r * IMAGEFORMAT_MAT[mat_id][2][0] + g * IMAGEFORMAT_MAT[mat_id][2][1] + b * IMAGEFORMAT_MAT[mat_id][2][2];
		}
	}

    _format = fmt;
}

Image* Image::rotate(const eImageRotation r) const
{
    return new Image(1, 0, _format);
}

Image* Image::scale(int32_t w, int32_t h) const
{
    return new Image(1, 0, _format);
}

Image* Image::crop(int32_t x, int32_t y, int32_t w, int32_t h) const
{
    Image* ret = new Image(w, h, _format);

    for (int32_t c = 0; c < _nch; c ++)
    {
        blit(ret, c, c, x, y, w, h, 0, 0);
    }

    return ret;
}

void Image::blit(Image* dest, int32_t ch_d, int32_t ch_s, int32_t x, int32_t y, int32_t w, int32_t h, int32_t dx, int32_t dy) const
{
    const float* s = _ch[ch_s];
    float* d = dest->_ch[ch_d];

    if (w > _w - x) w = _w - x;
    if (w > dest->_w) w = dest->_w;
    if (h > _h - y) h = _h - y;
    if (h > dest->_h) h = dest->_h;    
    
    for (int32_t i = 0; i < h; i ++)
    {
        memcpy(d + (dy + i) * dest->_w + dx, s + (y + i) * _w + x, w * sizeof (float));
    }
}

void m3x3Multiply(float* result, const float* A, const float* B)
{
	result[0] = A[0] * B[0] + A[3] * B[1] + A[6] * B[2];
	result[1] = A[1] * B[0] + A[4] * B[1] + A[7] * B[2];
	result[2] = A[2] * B[0] + A[5] * B[1] + A[8] * B[2];
	result[3] = A[0] * B[3] + A[3] * B[4] + A[6] * B[5];
	result[4] = A[1] * B[3] + A[4] * B[4] + A[7] * B[5];
	result[5] = A[2] * B[3] + A[5] * B[4] + A[8] * B[5];
	result[6] = A[0] * B[6] + A[3] * B[7] + A[6] * B[8];
	result[7] = A[1] * B[6] + A[4] * B[7] + A[7] * B[8];
	result[8] = A[2] * B[6] + A[5] * B[7] + A[8] * B[8];
}

float m3x3Determinant(const float* A)
{
	return	+A[0] * (A[4] * A[8] - A[7] * A[5])
			-A[1] * (A[3] * A[8] - A[5] * A[6])
			+A[2] * (A[3] * A[7] - A[4] * A[6]);
}

void m3x3Normalize(float* result, const float* A)
{
    const float invdet = 1.0f / m3x3Determinant(A);
	for (int32_t i = 0; i < 9; i ++) result[i] = A[i] * invdet;
}

void m3x3Inverse(float* result, const float* A)
{
    const float invdet = 1.0f / m3x3Determinant(A);

    result[0] =  (A[4] * A[8] - A[7] * A[5]) * invdet;
    result[1] = -(A[1] * A[8] - A[2] * A[7]) * invdet;
    result[2] =  (A[1] * A[5] - A[2] * A[4]) * invdet;
    result[3] = -(A[3] * A[8] - A[5] * A[6]) * invdet;
    result[4] =  (A[0] * A[8] - A[2] * A[6]) * invdet;
    result[5] = -(A[0] * A[5] - A[3] * A[2]) * invdet;
    result[6] =  (A[3] * A[7] - A[6] * A[4]) * invdet;
    result[7] = -(A[0] * A[7] - A[6] * A[1]) * invdet;
    result[8] =  (A[0] * A[4] - A[3] * A[1]) * invdet;
}

void m3x3Interpolate(float* result, const float* A, const float* B, float s)
{
	const float is = 1 - s;
	
	for (int32_t i = 0; i < 9; i ++)
		result[i] = (A[i] * is + B[i] * s);
}

int omp_get_num_threads()
{
	return 1;
}

int omp_get_thread_num()
{
	return 0;
}