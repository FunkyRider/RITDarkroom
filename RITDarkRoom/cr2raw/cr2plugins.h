#ifndef _CR2_PLUGINS_
#define _CR2_PLUGINS_

#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <memory.h>
#include "cr2raw.h"
#include "image.h"
#include "denoise.h"
#include "resample.h"
#include "array.h"

#include <stdint.h>

#ifdef WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

struct CAVal
{
	int focal;
	float r, b;
};

struct LensInfo {
	const char*		model;
    const char*     dispName;
    int32_t         id;
	CAVal	        ca[8];
};

struct CamBlackSat
{
    int             black;
    int             saturation;
    int             iso[12];
};

struct CamInfo
{
	const char* model;
	int32_t gbgb, nr;
	int32_t black;
    int32_t saturation;
    float wb_d50[3];
    float wb_stda[3];
    float cam_d50[9];		// 5000K
    float cam_stda[9];		// 2800K
    CamBlackSat* ext_blksat;
    int32_t ext_count;
};

const char* getAppPath();
int CR2_LoadLensInfo(const char* name, Array<LensInfo*> &data);
int CR2_LoadCamInfo(const char* name, Array<CamInfo*> &data);
void CR2_removeHotPixels(CR2Raw* raw);
void CR2_ADCColumnBias(CR2Raw* raw);
float m3x3ColorEqualize(float* m);

void CR2_GetCamBlackSaturation(const CamInfo &info, int iso, int32_t &black, int32_t &saturation);
int32_t CR2_CalculateBlack(CR2Raw* raw);

Image* CR2_correctca(Image* img, float r, float b, const uint16_t* sensorInfo);
Image* CR2_denoise(Image* img, float sv, float lv);
Image* CR2_colorTransform(Image* img, const float* from, const float* to, const float* fwb, const float* twb);

int CR2_get_lens_index(CR2Raw* raw, const Array<LensInfo*> &lensdb);
Image* CR2_profiled_lens(Image* img, CR2Raw* raw, const Array<LensInfo*> &lensdb, int& lindex);
Image* CR2_profiled_denoise(Image* img, int iso, bool baseline_nr);
Image* CR2_profiled_color(Image* img, const CamInfo* from, const CamInfo* to, const float dlmix, const float green, const float rgreen);

void CR2_white_point(const CamInfo* camfrom, const CamInfo* camto, const float* from, float* to);


#endif
