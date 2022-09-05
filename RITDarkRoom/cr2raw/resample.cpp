#include "stdafx.h"
#include <math.h>
#include "image.h"
//#include <omp.h>
#include <xmmintrin.h>
#include "resample.h"

__inline float scale(const int32_t i, const int32_t size, const float scale_val, const float shift_val)
{
	const float d = (i - size / 2) * scale_val + size / 2 - shift_val;
    const __m128 v = _mm_max_ss(_mm_set_ss(d), _mm_set_ss(0.0f));    
    return _mm_cvtss_f32(_mm_min_ss(v, _mm_set_ss(size - 1.0f)));
	/*
	if (d <= 0.0f)
		return 0.0f;
	else if (d >= size - 1)
		return (float) size - 1;
	else
		return d;
	*/
}

__inline float cubic(const float xm1, const float x, const float xp1, const float xp2, const float dx)
{
	return ((((-xm1 + 3.0f * x - 3.0f * xp1 + xp2) * dx +
                 (2.0f * xm1 - 5.0f * x + 4.0f * xp1 - xp2)) * dx +
                                (-xm1 + xp1)) * dx + (x + x)) * 0.5f;
}

Image* Resample_Channel(Image* img, const int ch, const float s, const float dx, const float dy, const float ratio)
{
    const int32_t w = img->width(), h = img->height();
    const float diagonal = sqrtf((float) (w * w + h * h));
    const float* src = img->channel(ch);
    float* dst = new float[w * h];
    const float scl = ratio > 0 ? ratio : (diagonal / (diagonal + 2.0f * s));

#pragma omp parallel for
    for (int32_t y = 0; y < h; y ++)
    {
        const float yfull = scale(y, h, scl, dy);
        const int yint2 = _mm_cvttss_si32(_mm_set_ss(yfull));
		//const int yint2 = (int) floorf(yfull);
        const float yfrac = yfull - yint2;

        const int yint1 = (yint2 == 0) ? yint2 : (yint2 - 1);
        const int yint3 = (yint2 == h - 1) ? yint2 : (yint2 + 1);
        const int yint4 = (yint3 == h - 1) ? yint3 : (yint3 + 1);

        const int row1 = yint1 * w, row2 = yint2 * w, row3 = yint3 * w, row4 = yint4 * w;
        const int rowd = y * w;

        for (int32_t x = 0; x < w; x ++)
        {
            const float xfull = scale(x, w, scl, dx);           
            const int xint2 = _mm_cvttss_si32(_mm_set_ss(xfull));
			//const int xint2 = (int) floorf(xfull);         
            const float xfrac = xfull - xint2;

            const int xint1 = (xint2 == 0) ? xint2 : (xint2 - 1);
            const int xint3 = (xint2 == w - 1) ? xint2 : (xint2 + 1);
            const int xint4 = (xint3 == w - 1) ? xint3 : (xint3 + 1);

            const float y1 = cubic(src[row1 + xint1], src[row1 + xint2], src[row1 + xint3], src[row1 + xint4], xfrac);
            const float y2 = cubic(src[row2 + xint1], src[row2 + xint2], src[row2 + xint3], src[row2 + xint4], xfrac);
            const float y3 = cubic(src[row3 + xint1], src[row3 + xint2], src[row3 + xint3], src[row3 + xint4], xfrac);
            const float y4 = cubic(src[row4 + xint1], src[row4 + xint2], src[row4 + xint3], src[row4 + xint4], xfrac);
            
            dst[rowd + x] = cubic(y1, y2, y3, y4, yfrac);
        }
    }
    
    delete[] img->swap(ch, dst);
    return img;
}
