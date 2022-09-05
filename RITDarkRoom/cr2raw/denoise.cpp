#include "stdafx.h"
#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include "denoise.h"
//#include <omp.h>
#include <xmmintrin.h>

Image* Denoise_Impulse(Image* img, int ch, const float threshold)
{
    const int32_t w = img->width(), h = img->height();
    float* d = new float[w * h];
    //int32_t hot = 0;
    const float* s = img->channel(ch);

    memcpy(d, s, w * sizeof (float));
    memcpy(d + w * (h - 1), s + w * (h - 1), w * sizeof (float));

    for (int32_t y = 0; y < h; y ++)
    {
        *(d + y * w) = *(s + y * w);
        *(d + y * w + w - 1) = *(s + y * w + w - 1);
    }

    for (int32_t y = 1; y < h - 1; y ++)
    {
        const int32_t l = y * w;
	    for (int32_t x = 1; x < w - 1; x ++)
        {
            const int32_t p = (l + x);
            const float val = s[p];
            const float avg = (s[p - 1] + s[p + 1] + s[p - w] + s[p + w] +
            				   s[p - w - 1] + s[p - w + 1] + s[p + w - 1] + s[p + w + 1]) * 0.125f;
                
            if ((val - avg) * (val - avg) > threshold)
            {
                d[p] = avg;
                //hot ++;
            }
            else
            {
                d[p] = val;
            }
        }
    }

    delete[] img->swap(ch, d);

    //printf("hot pixels: %d\n", hot);
    return img;
}

static void hat_transform_col(float *temp, float *base, uint32_t st, uint32_t size, uint32_t sc)
{
    uint32_t i;

    for (i = 0; i < sc; i ++)
        temp[i] = 2 * base[st * i] + base[st * (sc - i)] + base[st * (i + sc)];
    for (; i + sc < size; i ++)
        temp[i] = 2 * base[st * i] + base[st * (i - sc)] + base[st * (i + sc)];
    for (; i < size; i ++)
        temp[i] = 2 * base[st * i] + base[st * (i - sc)] + base[st * (2 * size - 2 - (i + sc))];
}

static void hat_transform_row(float *temp, float *base, uint32_t size, uint32_t sc)
{
    uint32_t i;

    for (i = 0; i < sc; i ++)
        temp[i] = 2 * base[i] + base[sc - i] + base[i + sc];
    for (; i + sc < size; i ++)
        temp[i] = 2 * base[i] + base[i - sc] + base[i + sc];
    for (; i < size; i ++)
        temp[i] = 2 * base[i] + base[i - sc] + base[2 * size - 2 - (i + sc)];
}

void wavelet_denoise(float *fimg[3], const uint32_t width, const uint32_t height, const float threshold, const float low, const DENOISE_MODE mode, const char* ch, const float contrast)
{
	const float lev_contrast[3][5] = {{0.0f, 0.1f, 0.1f, 0.2f, 0.2f}, {0.0f, 0.0f, 0.0f, 0.1f, 0.2f}, {0.0f, 0.1f, 0.1f, 0.2f, 0.2f}};
    const float lev_thold[3][5] = {{1.0f, 0.9f, 0.78f, 0.55f, 0.3f}, {1.0f, 0.9f, 0.75f, 0.8f, 0.4f}, {1.0f, 0.9f, 0.8f, 0.7f, 0.6f}};
    const float lev_low[3][5] = {{0.6f, 0.9f, 1.0f, 1.2f, 1.5f}, {0.8f, 0.9f, 1.0f, 0.6f, 1.5f}, {0.6f, 0.7f, 0.8f, 0.9f, 1.0f}};
    const double th_intensity[3][5] = {{1.0, 0.9, 0.7, 0.4, 0.1}, {1.0, 0.9, 0.8, 0.7, 0.6}, {1.0, 0.9, 0.8, 0.85, 0.7}};
    const int32_t size = width * height;
    const float val_multiplier = (mode == DENOISE_CHROMA) ? 2.5f : 1.0f;
    const float valpp = val_multiplier * 5.5f;

    uint32_t lpass = 0, hpass = 0;
    double stdev[5];
    uint32_t samples[5];
    float fdev[5], flow[5];
    float* fimg0 = fimg[0];
    float* temp = (float *) malloc ((width > height ? width : height) * sizeof (float));

    // lev 0: Highest frequency, lev 5: Lowest frequency
    for (int32_t lev = 0; lev < 5; lev ++)
    {
        const float th_scaled = threshold * lev_thold[mode][lev];
        const float tlow_scaled = low * lev_low[mode][lev];
        const float th_contrast = 1.0f + contrast * lev_contrast[mode][lev];
        const uint32_t sc = 1 << lev;
        lpass = ((lev & 1) + 1);
        float* fhpass = fimg[hpass];
        float* flpass = fimg[lpass];

        for (uint32_t row = 0; row < height; row ++)
	    {
            const uint32_t rwidth = row * width;

	        hat_transform_row(temp, fhpass + rwidth, width, sc);
	        for (uint32_t col = 0; col < width; col++)
	        {
	            flpass[rwidth + col] = temp[col] * 0.25f;
	        }
	    }

        for (uint32_t col = 0; col < width; col ++)
	    {
	        hat_transform_col(temp, flpass + col, width, height, sc);
	        for (uint32_t row = 0; row < height; row++)
	        {
	            flpass[row * width + col] = temp[row] * 0.25f;
	        }
	    }

        const float thold = 5.0f / (1 << 6) * exp (-2.6f * sqrtf (lev + 1.0f)) * 0.8002f / exp (-2.6f);

        for (int32_t i = 0; i < 5; i ++)
        {
            stdev[i] = 0;
            samples[i] = 0;
        }

        /* calculate standard-deviations */
        for (int32_t i = 0; i < size; i ++)
        {
	        fhpass[i] -= flpass[i];
	        if (fhpass[i] < thold && fhpass[i] > -thold)
	        {
                const float val = fabs(flpass[i]) * valpp;
                const int32_t v = _mm_cvtt_ss2si(_mm_set_ss(val));
                //const int32_t v = (int32_t) val;
                const int32_t vt = v > 4 ? 4 : v;

                stdev[vt] += fabs(flpass[i]) * fabs(fhpass[i]);
                samples[vt] ++;
	        }
        }

        for (int32_t i = 0; i < 5; i ++)
        {
            fdev[i] = (float) (sqrt(fabs(stdev[i] / (double)(samples[i] + 1))) * th_scaled * th_intensity[mode][i]);
            flow[i] = fdev[i] - fdev[i] * tlow_scaled;
        }

/*        
#pragma omp critical
        {
	        printf("%s%d: %.6f\t%.6f\t%.6f\t%.6f\t%.6f\n", ch, (lev + 1), (float) fdev[0], (float) fdev[1], (float) fdev[2], (float) fdev[3], (float) fdev[4]);
        }
*/

        /* do thresholding */
        for (int32_t i = 0; i < size; i ++)
        {
	        const float val = fabs(flpass[i]) * valpp;
            const int32_t v = _mm_cvtt_ss2si(_mm_set_ss(val));
            //const int32_t v = (int32_t) val;
            const int32_t vt = v > 4 ? 4 : v;
            const float thold = fdev[vt];
            const float tlow = flow[vt];
        	float pix = fhpass[i];
			
			
	        if (pix < -thold)
	        {
	            pix = (pix + tlow) * th_contrast;
	        }
	        else if (pix > thold)
	        {
	            pix = (pix - tlow) * th_contrast;
	        }
	        else
	        {
	            pix = pix * tlow_scaled;
	            if (pix < 0.03f && pix > -0.03f)
		            pix *= 0.8f;
	            if (pix < 0.015f && pix > -0.015f)
		            pix *= 0.8f;
	        }

			fhpass[i] = pix;
			
	        if (hpass)
	            fimg0[i] += pix;
        }

        hpass = lpass;
    }

    const float* flpass = fimg[lpass];
    for (int32_t i = 0; i < size; ++ i)
    {
        fimg0[i] += flpass[i];
    }

    free (temp);
}

Image* Denoise_Wavelet(Image* img, float yamt, float ylow, float cbcramt, float cbcrlow, float contrast)
{
    const int32_t w = img->width(), h = img->height();
    float amt[3] = {yamt, cbcramt, cbcramt};
    float low[3] = {ylow, cbcrlow, cbcrlow};
    DENOISE_MODE mode[3] = {DENOISE_LUMA, DENOISE_CHROMA, DENOISE_CHROMA};
    const char* name[3] = {"y", "b", "r"};

    //img->toFormat(eImageFormat_YCbCr);
	int cstart = (yamt > 0) ? 0 : 1;

#pragma omp parallel for
	for (int32_t i = cstart; i < 3; i ++)
	{
    	if (amt[i] > 0)
    	{
			float* buf[3];
			buf[0] = img->channel(i);
			buf[1] = (float*) malloc(w * h * sizeof (float));
			buf[2] = (float*) malloc(w * h * sizeof (float));
			wavelet_denoise(buf, w, h, amt[i], low[i], mode[i], name[i], contrast);
			free(buf[1]);
			free(buf[2]);
		}
	}

    return img;
}
