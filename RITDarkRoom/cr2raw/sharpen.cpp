#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
//#include <omp.h>
#include <xmmintrin.h>
#include <smmintrin.h>
#include <math.h>
#include "sharpen.h"

// RGB: sharpen 3 channels
// YCbCr: sharpen only Y channel
Image* Sharpen(Image* img, const float amount)
{
	const float convmatrix[4] = {-amount, 1.0f + amount * 2.0f, -amount, 0.0f};
	const int32_t w = img->width(), h = img->height();
	const int32_t ch_count = (img->format() == eImageFormat_RGB ? 3 : 1);
	Image* tmp = new Image(w, h, img->format());
	
	for (int32_t i = 0; i < ch_count; i ++)
	{
	    memcpy(tmp->channel(i), img->channel(i), sizeof (float) * w);
	    memcpy(tmp->channel(i) + (h - 1) * w, img->channel(i) + (h - 1) * w, sizeof (float) * w);
	}
	
	__m128 mat = _mm_loadu_ps(convmatrix);
	
#pragma omp parallel for
    for (int32_t c = 0; c < ch_count; c ++)
    {
        const float* sch = img->channel(c);
        float* tch = tmp->channel(c);
        
	    for (int32_t y = 1; y < h - 1; y ++)
	    {
	        const float* sp = sch + y * w;
	        float* tp = tch + y * w;
	        
	        for (int32_t x = 0; x < w; x ++)
	        {
	            __m128 pix = _mm_setr_ps(sp[x - w], sp[x], sp[x + w], 0.0f);
	            tp[x] = _mm_cvtss_f32(_mm_dp_ps(pix, mat, 0x71));
	        }
	    }
    }
    
#pragma omp parallel for
    for (int32_t c = 0; c < ch_count; c ++)
    {
        const float* sch = tmp->channel(c);
        float* tch = img->channel(c);
        
        for (int32_t y = 0; y < h; y ++)
        {
	        const float* sp = sch + y * w + 1;
	        float* tp = tch + y * w + 1;
                    
            for (int32_t x = 1; x < w - 1; x ++)
            {
	            __m128 pix = _mm_setr_ps(sp[x - 1], sp[x], sp[x + 1], 0.0f);
	            tp[x] = _mm_cvtss_f32(_mm_dp_ps(pix, mat, 0x71));                
            }
        }        
    }
    
    delete tmp;
    
    return img;
}

Image* Sharpen2(Image* img, const float samount, const float bamount, const float threshold)
{
    const float sharpmatrix[4] = {-samount, 1.0f + samount * 2.0f, -samount, 0.0f};
    const float blurmatrix[4] = {bamount, 1.0f - bamount * 2.0f, bamount, 0.0f};
	const int32_t w = img->width(), h = img->height();
	const int32_t ch_count = (img->format() == eImageFormat_RGB ? 3 : 1);
	const int32_t size = w * h;
	
	float* illum = new float[size];
	
	if (img->format() == eImageFormat_YCbCr)
	{
	    memcpy(illum, img->channel(0), sizeof (float) * size);
	}
	else
	{
	    float* ch[3] = {img->channel(0), img->channel(1), img->channel(2)};
	    
        const __m128 toY = _mm_setr_ps(0.299f, 0.587f, 0.114f, 0.0f);
	    for (int32_t i = 0; i < size; i ++)
	    {
            const __m128 rgb = _mm_setr_ps(ch[0][i], ch[1][i], ch[2][i], 0.0f);
	        illum[i] = _mm_cvtss_f32(_mm_dp_ps(rgb, toY, 0x71));
	    }
	}

    Image* tmp = new Image(w, h, img->format());
	
	for (int32_t i = 0; i < ch_count; i ++)
	{
	    memcpy(tmp->channel(i), img->channel(i), sizeof (float) * w);
	    memcpy(tmp->channel(i) + w, img->channel(i) + w, sizeof (float) * w);
        memcpy(tmp->channel(i) + (h - 1) * w, img->channel(i) + (h - 1) * w, sizeof (float) * w);
        memcpy(tmp->channel(i) + (h - 2) * w, img->channel(i) + (h - 2) * w, sizeof (float) * w);
    }
	
	__m128 mats = _mm_loadu_ps(sharpmatrix);
	__m128 matb = _mm_loadu_ps(blurmatrix);
	
#pragma omp parallel for
    for (int32_t c = 0; c < ch_count; c ++)
    {
        const float* sch = img->channel(c);
        float* tch = tmp->channel(c);
        
	    for (int32_t y = 2; y < h - 2; y ++)
	    {
	        const float* ip = illum + y * w;
	        const float* sp = sch + y * w;
	        float* tp = tch + y * w;
	        
	        for (int32_t x = 0; x < w; x ++)
	        {
	            const float yl = ip[x - w * 2], y0 = ip[x - w], y1 = ip[x], y2 = ip[x + w], yr = ip[x + w * 2];	        
	            const float sd = sqrtf(fabs((y0 - yl) * fabs(y0 - yl) * 0.7f + (y1 - y0) * fabs(y1 - y0) * 1.3f + (y2 - y1) * fabs(y2 - y1) * 1.3f + (yr - y2) * fabs(yr - y2) * 0.7f));
                float th = ((float) (sd - threshold) / (float) threshold) * 0.5f + 0.5f + (y0 + y1 + y2) * 0.06f;
                if (th > 1.0f) th = 1.0f;
                const float ith = 1.0f - th;

                __m128 mat = _mm_set1_ps(th);
                __m128 imat = _mm_set1_ps(ith);
                
                mat = _mm_mul_ps(mat, mats);
                imat = _mm_mul_ps(imat, matb);

	            __m128 pix = _mm_setr_ps(sp[x - w], sp[x], sp[x + w], 0.0f);
	            tp[x] = _mm_cvtss_f32(_mm_dp_ps(pix, _mm_add_ps(mat, imat), 0x71));
	        }
	    }
    }
    
#pragma omp parallel for
    for (int32_t c = 0; c < ch_count; c ++)
    {
        const float* sch = tmp->channel(c);
        float* tch = img->channel(c);
        
        for (int32_t y = 0; y < h; y ++)
        {
            const float* ip = illum + y * w;
	        const float* sp = sch + y * w;
	        float* tp = tch + y * w;
            
            for (int32_t x = 2; x < w - 2; x ++)
            {
	            const float yl = ip[x - 2], y0 = ip[x - 1], y1 = ip[x], y2 = ip[x + 1], yr = ip[x + 2];	        
	            const float sd = sqrtf(fabs((y0 - yl) * fabs(y0 - yl) * 0.7f + (y1 - y0) * fabs(y1 - y0) * 1.3f + (y2 - y1) * fabs(y2 - y1) * 1.3f + (yr - y2) * fabs(yr - y2) * 0.7f));
                float th = ((float) (sd - threshold) / (float) threshold) * 0.5f + 0.5f + (y0 + y1 + y2) * 0.06f;
                if (th > 1.0f) th = 1.0f;
                const float ith = 1.0f - th;

                __m128 mat = _mm_set1_ps(th);
                __m128 imat = _mm_set1_ps(ith);
                
                mat = _mm_mul_ps(mat, mats);
                imat = _mm_mul_ps(imat, matb);

	            __m128 pix = _mm_setr_ps(sp[x - 1], sp[x], sp[x + 1], 0.0f);
	            tp[x] = _mm_cvtss_f32(_mm_dp_ps(pix, _mm_add_ps(mat, imat), 0x71));
            }        
        }
    }

	delete[] illum;
    delete tmp;
	
	return img;
}
