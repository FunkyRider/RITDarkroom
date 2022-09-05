#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <memory.h>
#include "image.h"
#include "cr2raw.h"
#include "SaveTiff.h"
#include "cr2plugins.h"

#ifdef USE_OMP
#include <omp.h>
#endif

#ifdef WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

const int32_t curve[] = 
	{0,2,6,10,14,18,22,26,30,34,38,42,46,50,54,58,62,66,70,74,78,82,86,90,94,98,
	102,106,110,114,118,122,126,130,134,138,142,146,150,154,158,162,166,170,174,
	178,182,186,190,194,198,202,206,210,214,218,222,226,230,234,238,242,246,250,
	254,258,262,266,270,274,278,282,286,290,294,298,304,308,312,316,320,324,328,
	332,336,340,346,350,354,360,364,368,374,378,384,388,394,400,404,410,416,420,
	426,432,438,444,452,458,464,472,478,486,494,500,510,518,526,534,542,552,560,
	568,578,586,596,604,614,624,632,642,652,662,672,682,692,704,714,726,736,748,
	758,770,782,794,806,820,834,846,860,874,888,904,920,936,952,970,988,1004,1022,
	1040,1058,1076,1094,1112,1130,1148,1166,1184,1202,1222,1240,1258,1276,1296,1314,
	1334,1352,1372,1390,1410,1428,1448,1468,1488,1506,1526,1546,1566,1586,1606,1626,
	1646,1668,1688,1708,1730,1752,1772,1794,1816,1840,1862,1886,1908,1932,1956,1982,
	2006,2032,2058,2084,2110,2138,2166,2194,2222,2250,2280,2310,2340,2370,2400,2430,
	2462,2492,2524,2554,2586,2618,2650,2682,2714,2746,2780,2812,2846,2880,2914,2948,
	2982,3018,3052,3088,3124,3160,3196,3232,3270,3306,3344,3382,3420,3460,3498,3538,
	3578,3585}; // 3585 max

const int32_t cam[] = {8016,-1694,-1204,-5796,13887,2230,-1784,1944,7671};
const float xyz_rgb[9] = 			/* XYZ from RGB */
  {0.412453f, 0.357580f, 0.180423f,
  0.212671f, 0.715160f, 0.072169f,
  0.019334f, 0.119193f, 0.950227f};
  
const uint16_t _BLACK = 1023, _SATURATION = 0x3bb0;

int main(int argc, char** argv)
{
	printf("Step 1...\n");
	// 1. Load/decode
	CR2Raw* src = new CR2Raw(argv[1]);
	const int32_t w = src->getWidth(), h = src->getHeight();
    uint16_t* data = src->getRawData();    
    const int32_t s = w * h;
    Image* bayer = new Image(w / 2, h / 2, eImageFormat_RGBG);
    bayer->fromRAW16(data, _SATURATION, _BLACK);
    
	printf("Step 2...\n");
    // 2. Demosaic
    Image* img = bayer->demosaicRAW(eImageDemosaic_Linear);
    delete bayer;
    
	printf("Step 3...\n");
    // 3. WB scale
    uint32_t* wb = src->getWBCoefficients();
    float fwb[3] = {(float) wb[0] / (float) wb[1], 1.0f, (float) wb[3] / (float) wb[1]};
    
    for (int32_t i = 0; i < 3; i ++)
    {
    	if (i != 1)
    	{
    		float* ch = img->channel(i);    		
    		for (int32_t j = 0; j < s; j ++)
    		{
    			//ch[j] *= fwb[i];
    		}    	
    	}    
    }
    
	printf("Step 4...\n");
    // 4. Color matrix: cam_xyz, xyz_rgb
    float cam_xyz[9], cam_rgb[9], cam_cam[9];
    
    for (int32_t i = 0; i < 9; i ++)
    	cam_xyz[i] = (float) cam[i] / 10000.0f;
    
    m3x3Multiply(cam_rgb, xyz_rgb, cam_xyz);
    m3x3Inverse(cam_cam, cam_rgb);
    
    float* ch[] = {img->channel(0), img->channel(1), img->channel(2)};
	for (int32_t i = 0; i < s; i ++)
    {
        const float r = ch[0][i];
        const float g = ch[1][i];
        const float b = ch[2][i];

        const float dr = (r * cam_cam[0] + g * cam_cam[1] + b * cam_cam[2]) / 2;
        const float dg = (r * cam_cam[3] + g * cam_cam[4] + b * cam_cam[5]) / 2;
        const float db = (r * cam_cam[6] + g * cam_cam[7] + b * cam_cam[8]) / 2;

        ch[0][i] = dr / 16383.0f * 3578.0f * 0.8;
        ch[1][i] = dg / 16383.0f * 3578.0f;
        ch[2][i] = db / 16383.0f * 3578.0f * 1.3;
    }
    
	printf("Step 5...\n");
    // 5. Tone curve
    uint16_t* rgb12 = new uint16_t[s * 3];
    img->toRGB16(rgb12);
    delete img;
    
    uint8_t tcurve[3590];    
    for (int32_t i = 0; i < 3590; i ++)
    {
    	for (int32_t j = 0; j < 256; j ++)
    	{
    		if (i <= curve[j])
    		{
    			tcurve[i] = j;
    			break;
    		}
    	}
    }
        
    uint8_t* rgb8 = new uint8_t[s * 3];    
    for (int32_t i = 0; i < s * 3; i ++)
    {
    	uint16_t v = rgb12[i];
    	if (v > 3578) v = 3578;
    	
    	int32_t p = tcurve[v];
    	rgb8[i] = p;
    }
    
    delete[] rgb12;
    
	printf("Step 6...\n");
    // 6. Save tiff
    SaveTIFF(rgb8, w, h, 24, "out.tif");
	delete[] rgb8;
    
    delete src;
    return 0;
}


		
int main2(int argc, char** argv)
{
	CR2_LoadCamInfo("cam_canon.txt", g_camInfo);
    CR2_LoadLensInfo("lens_canon.txt", g_lensInfo);
    
    printf("Load...\n");

    CR2Raw* raw = new CR2Raw(argv[1]);
    Image* img = new Image(raw->getWidth() / 2, raw->getHeight() / 2, eImageFormat_RGBG);
    img->fromRAW16(raw->getRawData(), 15280, 1024);
    
    img = CR2_profiled_lens(img, raw, g_lensInfo);
    
    //const float wb_d50[] = {2.36f, 1.0f, 1.33f};
    uint32_t* wbraw = raw->getWBCoefficients();
    const float wb_shot[] = {(float) wbraw[0] / (float) wbraw[1], 1.0f, (float) wbraw[3] / (float) wbraw[1]};
        
	delete raw;


    printf("Demosaicing...\n");
	Image* dimg = img->demosaicRAW(eImageDemosaic_AHD);
    delete img;
    int32_t s = dimg->width() * dimg->height();
    
    dimg->scale(wb_shot);
    
	uint8_t tcurve[3590];
	static const int32_t _curve[] = 
		{0,2,6,10,14,18,22,26,30,34,38,42,46,50,54,58,62,66,70,74,78,82,86,90,94,98,
		102,106,110,114,118,122,126,130,134,138,142,146,150,154,158,162,166,170,174,
		178,182,186,190,194,198,202,206,210,214,218,222,226,230,234,238,242,246,250,
		254,258,262,266,270,274,278,282,286,290,294,298,304,308,312,316,320,324,328,
		332,336,340,346,350,354,360,364,368,374,378,384,388,394,400,404,410,416,420,
		426,432,438,444,452,458,464,472,478,486,494,500,510,518,526,534,542,552,560,
		568,578,586,596,604,614,624,632,642,652,662,672,682,692,704,714,726,736,748,
		758,770,782,794,806,820,834,846,860,874,888,904,920,936,952,970,988,1004,1022,
		1040,1058,1076,1094,1112,1130,1148,1166,1184,1202,1222,1240,1258,1276,1296,1314,
		1334,1352,1372,1390,1410,1428,1448,1468,1488,1506,1526,1546,1566,1586,1606,1626,
		1646,1668,1688,1708,1730,1752,1772,1794,1816,1840,1862,1886,1908,1932,1956,1982,
		2006,2032,2058,2084,2110,2138,2166,2194,2222,2250,2280,2310,2340,2370,2400,2430,
		2462,2492,2524,2554,2586,2618,2650,2682,2714,2746,2780,2812,2846,2880,2914,2948,
		2982,3018,3052,3088,3124,3160,3196,3232,3270,3306,3344,3382,3420,3460,3498,3538,
		3578,3585};	
    for (int32_t i = 0; i < 3590; i ++)
	{
		tcurve[i] = 255;
		for (int32_t j = 0; j < 256; j ++)
		{
			if (i <= _curve[j])
			{
				tcurve[i] = j;
				break;
			}
		}
	}
    
    const float* mat = g_camInfo[1]->cam_d50;
    float *cr = dimg->channel(0);
    float *cg = dimg->channel(1);
    float *cb = dimg->channel(2);

	uint8_t* data = (uint8_t*) malloc(s * 3);
	uint8_t* p = data;

    for (int32_t i = 0; i < s; i ++)
    {
    	float r = cr[i] * mat[0] + cg[i] * mat[3] + cb[i] * mat[6];
    	float g = cr[i] * mat[1] + cg[i] * mat[4] + cb[i] * mat[7];
    	float b = cr[i] * mat[2] + cg[i] * mat[5] + cb[i] * mat[8];
    	
    	//cr[i] = r;
    	//cg[i] = g;
    	//cb[i] = b;
    	
    	r *= 1.3f; g *= 1.3f; b *= 1.3f;
    	
		int32_t tr = (r * 3589.0f), tg = (g * 3589.0f), tb = (b * 3589.0f);
		if (tr < 0) tr = 0; else if (tr > 3589) tr = 3589;
		if (tg < 0) tg = 0; else if (tg > 3589) tg = 3589;
		if (tb < 0) tb = 0; else if (tb > 3589) tb = 3589;
        *p ++ = tcurve[tr];
        *p ++ = tcurve[tg];
        *p ++ = tcurve[tb];
    }

    printf("Saving...\n");
	//dimg->toRGB8(data);

	SaveTIFF(data, dimg->width(), dimg->height(), 24, "out.tif");
	free(data);	
	
	
	return 0;
}


/*
{
   // Rescale from panasonic 127-4097 to Canon 12-bit 127-3712
    const float satval = blksatwb[3] - blksatwb[0];
    
    for (int32_t i = 0; i < sh; i ++)
    {
        uint16_t *d = sdata + i * sw;
        for (int32_t j = 0; j < sw; j ++)
        {
            float fv = (float) d[j];

            //fv = (15280.0f - 1023.0f) * fv / 4097.0f + 1023.0f;            
            fv = (3692.0f - 127.0f) * fv / satval + _template_cam[template_id].black;

            uint32_t v = (uint32_t) fv;
            //if (v > 0x3bb0) v = 0x3bb0;            
			if (v < 0) v = 0;
            else if (v > 0xe6c) v = 0xe6c;
            
            d[j] = v;
        }
    }

    //uint16_t* sinfo = src->getSensorInfo();    
    //sinfo[5] -= 2;
    //sinfo[7] -= 2;    
    //FORC(12) printf("%d ", sinfo[c]);

    float wbbase[3];
    {
		float cam[9], tem[9];
		FORC(9)
		{
			cam[c] = (float) _cam[id].cam_xyz[c] / 10000.0f;
			tem[c] = (float) _template_cam[template_id].cam_xyz[c] / 10000.0f;
		}
    	
    	FORC3
    	wbbase[c] = sqrtf(cam[3 * c + 0] * cam[3 * c + 0] + cam[3 * c + 1] * cam[3 * c + 1] + cam[3 * c + 2] * cam[3 * c + 2]) /
    				sqrtf(tem[3 * c + 0] * tem[3 * c + 0] + tem[3 * c + 1] * tem[3 * c + 1] + tem[3 * c + 2] * tem[3 * c + 2]);
    }
    
    FORC3 printf("%f ", wbbase[c]);
    
    // Approximate WB translation
    uint32_t* wb = src->getWBCoefficients();
    //wb[0] = 2048;   wb[1] = 1024;   wb[2] = 1024;   wb[3] = 1536;    
    float wbscale = (float) wb[1] / (float) blksatwb[7];
	wb[0] = (uint32_t)(blksatwb[6] * wbscale);
	wb[3] = (uint32_t)(blksatwb[8] * wbscale);
	
	//FORC(4) printf("%d ", wb[c]);
}
*/

/*
int main2(int argc, char** argv)
{
    main(argc, argv);

    printf("Load...\n");

    char path[512];
    strcpy(path, getAppPath());
	strcat(path, "Pana_out.CR2");

    CR2Raw* raw = new CR2Raw(path);
    Image* img = new Image(raw->getWidth() / 2, raw->getHeight() / 2, eImageFormat_RGBG);
    img->fromRAW16(raw->getRawData(), 12, 127);
	delete raw;

    printf("Demosaicing...\n");
	Image* dimg = img->demosaicRAW(eImageDemosaic_Linear);    
    delete img;
    int32_t s = dimg->width() * dimg->height();

    printf("Saving...\n");
	uint8_t* data = (uint8_t*) malloc(s * 3);
	dimg->toRGB8(data);
	SaveTIFF(data, dimg->width(), dimg->height(), 24, "D:\\Private\\cr2raw\\Debug\\out.tif");
	free(data);	
	
	
	return 0;
}
*/
