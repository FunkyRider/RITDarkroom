#include "cr2plugins.h"
#include "ustring.h"
#include "os.h"
//#include <omp.h>

static int readLine(FILE* fp, char* ptr, int size)
{
	char* p = ptr;
	char buf = 0;
	
	while (p - ptr < (size - 1))
	{
		buf = fgetc(fp);
		
		if (buf == -1 || buf == 10)
		{
			*p = 0;
			return p - ptr;
		}
		
		if (buf == 13)
			continue;
		
		*p ++ = buf;		
	}
	
	*p = 0;
	
	return p - ptr;
}

int CR2_LoadLensInfo(const char* name, Array<LensInfo*> &data)
{
    char buf[512];
    const char *path = getAppPath();	

	strcpy(buf, path);
	strcat(buf, name);

	FILE* fp = _fopen(buf, "rb");
    if (!fp)
 		return 0;

    LensInfo* lens = 0;
    int step = 0;
    int vMaj = 0, vMin = 0;

    while (!feof(fp))
	{
		readLine(fp, buf, 512);
		
		if (buf[0] == 0 || buf[0] == '#')
        {            
            step = 0;
            if (lens != 0)
            {
                data.add(lens);
                lens = 0;
            }
			continue;
        }

        String line(buf);
		size_t p = 0;

        if (vMaj == 0 && vMin == 0)
        {            
            if (line.term(p) != "Version")
            {
                printf("Error: No version in profile %s, skipping...\n", name);
                return -1;
            }

            vMaj = line.term(p, L'.').toInt();
            vMin = line.term(p).toInt();

            if (vMaj > 1 || (vMaj == 1 && vMin > 0))
            {
                printf("Error: Profile %s version %d.%d is too high, upgrade me\n", name, vMaj, vMin);
                return -1;
            }

            continue;
        }
		
		switch (step)
		{
		case 0:
			{
                lens = new LensInfo();
                char *name = new char[line.length() + 1];
				strcpy(name, line.c_str());			
				lens->model = name;
				step = 1;
            }
            break;
        case 1:
            {
                char *name = new char[line.length() + 1];
				strcpy(name, line.c_str());			
				lens->dispName = name;
				step = 2;
            }
            break;
        case 2:
            {
                lens->id = line.term(p).toInt();
                step = 3;
            }
            break;
        default:
            {
                int caid = step - 3;

                if (caid < 8)
                {
                    CAVal &ca = lens->ca[caid];
                    
                    ca.focal = line.term(p).toInt();
                    ca.r = line.term(p).toFloat();
                    ca.b = line.term(p).toFloat();
                }
                else
                {
                    printf("Warning: Lens CA data cannot exceed 8 items:\n    %s\n", lens->dispName);
                }

                step ++;
            }
        }
    }

    fclose(fp);

	return 0;
}

int CR2_LoadCamInfo(const char* name, Array<CamInfo*> &data)
{
	char buf[512];
	const char *path = getAppPath();
	
	strcpy(buf, path);
	strcat(buf, name);

	FILE* fp = _fopen(buf, "rb");
 	if (!fp)
 		return 0;

    int vMaj = 0, vMin = 0;
	CamInfo* cam = 0;
	int step = 0;
    int cext_blksat = 0, iext_blksat = 0;

	while (!feof(fp))
	{
		readLine(fp, buf, 512);
		
		if (buf[0] == 0 || buf[0] == '#')
			continue;
		
		String line(buf);
		size_t p = 0;

        if (vMaj == 0 && vMin == 0)
        {            
            if (line.term(p) != "Version")
            {
                printf("Error: No version in profile %s, skipping...\n", name);
                return -1;
            }

            vMaj = line.term(p, L'.').toInt();
            vMin = line.term(p).toInt();

            if (vMaj > 1 || (vMaj == 1 && vMin > 1))
            {
                printf("Error: Profile %s version %d.%d is too high, upgrade me\n", name, vMaj, vMin);
                return -1;
            }

            continue;
        }
		
		switch (step)
		{
		case 0:
            if (cext_blksat == 0)
			{
				cam = new CamInfo();
				char *name = new char[line.length() + 1];
                cam->ext_blksat = NULL;
                cam->ext_count = 0;
				strcpy(name, line.c_str());			
				cam->model = name;
				step = 1;
			}
            else if (cam)
            {
                // Load extended black / saturation table
                if (!cam->ext_blksat)
                {
                    cam->ext_blksat = new CamBlackSat[cext_blksat];
                    cam->ext_count = cext_blksat;
                    memset(cam->ext_blksat, 0, sizeof (CamBlackSat) * cext_blksat);
                }

                cam->ext_blksat[iext_blksat].black = line.term(p).toInt();
                cam->ext_blksat[iext_blksat].saturation = line.term(p).toInt();
                
                for (int i = 0; i < 12 && p < line.length(); i ++)
                {
                    cam->ext_blksat[iext_blksat].iso[i] = line.term(p).toInt();
                }

                iext_blksat ++;
                cext_blksat --;
            }
			break;
		case 1:
			{
				cam->gbgb = line.term(p).toInt();
				cam->nr = line.term(p).toInt();
				cam->black = line.term(p).toInt();
				cam->saturation = line.term(p).toInt();

                if (p < line.length())
                {
                    cext_blksat = line.term(p).toInt();
                    iext_blksat = 0;
                }

				step = 2;
			}
			break;
		case 2:
			{
				cam->wb_d50[0] = line.term(p).toFloat();
				cam->wb_d50[1] = line.term(p).toFloat();
				cam->wb_d50[2] = line.term(p).toFloat();
				cam->wb_stda[0] = line.term(p).toFloat();
				cam->wb_stda[1] = line.term(p).toFloat();
				cam->wb_stda[2] = line.term(p).toFloat();
				step = 3;
			}
			break;
		case 3:
			{
				for (int i = 0; i < 9; i ++)
					cam->cam_d50[i] = line.term(p).toFloat();
				
                if (cam->cam_d50[0] != 0)
                {
                    const float r = cam->cam_d50[0] + cam->cam_d50[3] + cam->cam_d50[6];
                    const float g = cam->cam_d50[1] + cam->cam_d50[4] + cam->cam_d50[7];
                    const float b = cam->cam_d50[2] + cam->cam_d50[5] + cam->cam_d50[8];
                    const float rdev = -(r - 1.0f) / 3.0f, gdev = -(g - 1.0f) / 3.0f, bdev = -(b - 1.0f) / 3.0f;

                    if (fabs(rdev) > 0.0034f || fabs(gdev) > 0.0034f || fabs(bdev) > 0.0034f)
                    {
                        printf("Warning: Cam_D50 white point not neutral:\n    %s: %f %f %f\n", cam->model, r, g, b);
                    }
                    else
                    {
                        cam->cam_d50[0] += rdev;    cam->cam_d50[3] += rdev;    cam->cam_d50[6] += rdev;
                        cam->cam_d50[1] += gdev;    cam->cam_d50[4] += gdev;    cam->cam_d50[7] += gdev;
                        cam->cam_d50[2] += bdev;    cam->cam_d50[5] += bdev;    cam->cam_d50[8] += bdev;
                    }
                }

                step = 4;
			}
			break;
		case 4:
			{
				for (int i = 0; i < 9; i ++)
					cam->cam_stda[i] = line.term(p).toFloat();
					
                if (cam->cam_stda[0] != 0)
                {
                    const float r = cam->cam_stda[0] + cam->cam_stda[3] + cam->cam_stda[6];
                    const float g = cam->cam_stda[1] + cam->cam_stda[4] + cam->cam_stda[7];
                    const float b = cam->cam_stda[2] + cam->cam_stda[5] + cam->cam_stda[8];
                    const float rdev = -(r - 1.0f) / 3.0f, gdev = -(g - 1.0f) / 3.0f, bdev = -(b - 1.0f) / 3.0f;

                    if (fabs(rdev) > 0.0034f || fabs(gdev) > 0.0034f || fabs(bdev) > 0.0034f)
                    {
                        printf("Warning: Cam_StdA white point not neutral:\n    %s: %f %f %f\n", cam->model, r, g, b);
                    }
                    else
                    {
                        cam->cam_stda[0] += rdev;    cam->cam_stda[3] += rdev;    cam->cam_stda[6] += rdev;
                        cam->cam_stda[1] += gdev;    cam->cam_stda[4] += gdev;    cam->cam_stda[7] += gdev;
                        cam->cam_stda[2] += bdev;    cam->cam_stda[5] += bdev;    cam->cam_stda[8] += bdev;
                    }
                }
				
                data.add(cam);
				step = 0;
			}
			break;
		}		
	}
	
	fclose(fp);
	
	return 0;
}

void CR2_GetCamBlackSaturation(const CamInfo &info, int iso, int32_t &black, int32_t &saturation)
{
    for (int i = 0; i < info.ext_count; i ++)
    {
        CamBlackSat& tbl = info.ext_blksat[i];
        for (int j = 0; j < 12; j ++)
        {
            if (tbl.iso[j] == 0)
                break;

            if (tbl.iso[j] == iso)
            {
                black = tbl.black;
                saturation = tbl.saturation;
                return;
            }
        }
    }

    black = info.black;
    saturation = info.saturation;
}

int32_t CR2_CalculateBlack(CR2Raw* raw)
{
    const uint16_t* bayer = raw->getRawData();
    const uint16_t* sensor = raw->getSensorInfo();
    const int w = sensor[1], l = sensor[5], t = sensor[6] + 8, b = sensor[8] - 8;

    if (l < 24)
        return 0;

    uint32_t v = 0;
    uint32_t s = 0;

    for (int y = t; y <= b; y ++)
    {
        for (int x = l - 24; x < l - 20; x ++)
        {
            v += bayer[y * w + x];
            s ++;
        }
    }

    return v / s;
}

void CR2_removeHotPixels(CR2Raw* raw)
{ 
	const int32_t vmax = (1 << raw->getBps()) - 1;
	const int32_t w = raw->getWidth();
 	char fname[512];
 	sprintf(fname, "%s_%s.Hot", raw->getModel(), raw->getSerialNumber());
 	const uint32_t flen = strlen(fname);
 	for (uint32_t i = 0; i < flen; i ++)
 	{
 		if (fname[i] == ' ') fname[i] = '_';
 	}

    char path[512];
	strcpy(path, getAppPath());
	strcat(path, fname); 	
 	FILE* fp = _fopen(path, "rt");
 	
 	if (!fp)
 		return;
 		
 	uint16_t* data = raw->getRawData();
 	
    printf("Hot pixel removal...");
    
 	int32_t j, i;
 	float mul;
 	
 	int hot = 0, bad = 0;
 	
    while (!feof(fp))
    {
    	if (fscanf(fp, "%d %d %f", &j, &i, &mul) < 3) break;
    	
	 	uint16_t &val = data[i * w + j];
		const int32_t avg = (data[(i - 2) * w + j - 2] +
			data[(i - 2) * w + j] +
			data[(i - 2) * w + j + 2] +
			data[i * w + j - 2] +
			data[i * w + j + 2] +
			data[(i + 2) * w + j - 2] +
			data[(i + 2) * w + j] +
			data[(i + 2) * w + j + 2]) / 8;
		
		if ((avg * 8) < vmax && mul < 4.0f && mul >= 1.0f)
		{
			// If hot pixel's sensitivity is less than 2 stops the ordinary value
			// and is in a dark region try to salvage it by bring the brightness
			// back to normal
			val = (uint16_t)((float) val / mul);
			hot ++;
		}
		else
		{
			val = avg;
			bad ++;
		}
    }
    
    fclose(fp);
    
    printf("%d bad, %d hot\n", bad, hot);
}

void CR2_ADCColumnBias(CR2Raw* raw)
{
	const int32_t dw = raw->getWidth(), dh = raw->getHeight();
 	char fname[512];
 	sprintf(fname, "%s_%s.ADCBias", raw->getModel(), raw->getSerialNumber());
 	const uint32_t flen = strlen(fname);
 	for (uint32_t i = 0; i < flen; i ++)
 	{
 		if (fname[i] == ' ') fname[i] = '_';
 	}

    char path[512];
	strcpy(path, getAppPath());
	strcat(path, fname); 	
 	FILE* fp = _fopen(path, "rt");

	int32_t CDS_COL[8] = {0, 0, 0, 0, 0, 0, 0, 0};
 	
 	if (!fp)
 		return;
 	
 	for (int32_t i = 0; i < 8; i ++)
 	{
 		int32_t v; 		
 		if (fscanf(fp, "%d", &v) < 1) break;
 		CDS_COL[i] = v;
 	}
 	
 	uint16_t* data = raw->getRawData(); 	
	
	printf("ADC Column bias: ");
	for (int32_t i = 0; i < 8; i ++)
	{
		printf("%d ", CDS_COL[i]);
	}
	printf("\n");

	for (int32_t y = 0; y < dh; y ++)
	{
		int32_t l = y * dw;
		for (int32_t x = 0; x < dw; x ++)
		{
			int32_t bias = CDS_COL[x % 8];
			int32_t v = data[l + x];
			
			v -= bias;							
			data[l + x] = (uint16_t) v;
		}
	}
		
 	fclose(fp);
}

float m3x3ColorEqualize(float* m)
{
	float delta = 0.0f;
	
	float r = m[0] + m[3] + m[6];
	float g = m[1] + m[4] + m[7];
	float b = m[2] + m[5] + m[8];
	
	const float dr = fabs(1.0f - r), dg = fabs(1.0f - g), db = fabs(1.0f - b);
	
	if (delta < dr) delta = dr;
	if (delta < dg) delta = dg;
	if (delta < db) delta = db;
		
	m[0] /= r;	m[3] /= r;	m[6] /= r;
	m[1] /= g;	m[4] /= g;	m[7] /= g;
	m[2] /= b;	m[5] /= b;	m[8] /= b;
	
	return delta;
}

Image* CR2_correctca(Image* img, float r, float b, const uint16_t* sensorInfo)
{
    printf("CA Correction (cr: %.3f, cb: %.3f)...\n", r, b);
    int32_t x1 = sensorInfo[5], x2 = sensorInfo[7], y1 = sensorInfo[6], y2 = sensorInfo[8];

    // Expand up to 2 pixels outside crop area to reduce shrink artifacts
    for (int32_t i = 0; i < 2 && x1 > 0 && y1 > 0 && (x2 + 1) < img->width() * 2 && (y2 + 1) < img->height() * 2; i ++)
    {
        x1 --; x2 ++;
        y1 --; y2 ++;
    }

    const int32_t w = (x2 - x1 + 1) / 2, h = (y2 - y1 + 1) / 2;    
    Image* crop = new Image(w, h, eImageFormat_Gray);
    
    img->blit(crop, 0, 0, x1 / 2, y1 / 2, w, h, 0, 0);
    Resample_Channel(crop, 0, r / 2, 0, 0, 0);
    //printf("Resample: s: %f dx: %f dy: %f\n", r / 2, 0.0f, 0.0f);
/*
{       
int s = img->width() * img->height();
float *rc = img->channel(0);
FORC(s) rc[c] = 0;
}
*/
    crop->blit(img, 0, 0, 0, 0, w, h, x1 / 2, y1 / 2);

    img->blit(crop, 0, 2, x1 / 2, y1 / 2, w, h, 0, 0);
    Resample_Channel(crop, 0, b / 2, 0, 0, 0);
    //printf("Resample: s: %f dx: %f dy: %f\n", b / 2, 0.0f, 0.0f);
/*
{
int s = img->width() * img->height();
float *rc = img->channel(2);
FORC(s) rc[c] = 0;
}
*/
    crop->blit(img, 2, 0, 0, 0, w, h, x1 / 2, y1 / 2);
/*
{
int s = img->width() * img->height();
float *rc = img->channel(1);
FORC(s) rc[c] = 0;
}
{
int s = img->width() * img->height();
float *rc = img->channel(3);
FORC(s) rc[c] = 0;
}
*/
    delete crop;
    return img;
}

int CR2_get_lens_index(CR2Raw* raw, const Array<LensInfo*> &lensdb)
{
    const size_t dbsize = lensdb.size();
	const char* clens = raw->getLensModel();
    char lens[64];
    strncpy(lens, clens, 64);
    int ll = (int) strlen(lens);
    while (ll > 0 && lens[ll - 1] == 32) lens[ll - 1] = 0;
    const int32_t lensID = raw->getLensID();
    
    for (uint32_t i = 0; i < dbsize; i ++)
    {
        if (strcmp(lensdb[i]->model, lens) == 0 || (lens[0] == 0 && lensID != 0 && lensdb[i]->id == lensID) ||
        	strcmp(lensdb[i]->model, raw->getModel()) == 0)
        {
        	return i;
        }
    }
    
    return -1;
}

Image* CR2_profiled_lens(Image* img, CR2Raw* raw, const Array<LensInfo*> &lensdb, int& lindex)
{
	int i = CR2_get_lens_index(raw, lensdb);
    const int32_t focal = raw->getFocalLength();
	
	if (i < 0)
		return img;
	
	lindex = i;
	
    printf("Lens: %s (%d)\n", (lensdb[i]->dispName[0] != 0) ? lensdb[i]->dispName : lensdb[i]->model, lensdb[i]->id);
    printf("Focal Length: %d\n", focal);

    const LensInfo &info = *lensdb[i];
    uint32_t count = COUNT(info.ca);

    float cr = info.ca[0].r;
    float cb = info.ca[0].b;

    for (uint32_t j = 1; j < count; j ++)
    {                    
        const int32_t h_f = info.ca[j].focal, l_f = info.ca[j - 1].focal;
        if (focal <= h_f)
        {
            const float s = (float) (focal - l_f) / (float) (h_f - l_f);
            cr = info.ca[j - 1].r * (1 - s) + info.ca[j].r * s;
            cb = info.ca[j - 1].b * (1 - s) + info.ca[j].b * s;
            break;
        }
    }

    const uint16_t* sinfo = raw->getSensorInfo();
    const int32_t x1 = sinfo[5], x2 = sinfo[7];
    const int32_t w = x2 - x1 + 1;
    const float sr = (w - w * cr) / 2, sb = (w - w * cb) / 2;

    img = CR2_correctca(img, sr, sb, sinfo);

    
    return img;
}

Image* CR2_denoise(Image* img, float sv, float lv)
{
    printf("Noise reduction (%.3f, %.3f)...\n", sv, lv);
	float amt[4] = {0.7f, 1.0f, 0.6f, 1.0f};
    float low[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    
    const uint32_t w = img->width(), h = img->height();
    
#pragma omp parallel for
	for (int32_t i = 0; i < 4; i ++)
	{
		float* buf[3];
		buf[0] = img->channel(i);
		buf[1] = (float*) malloc(w * h * sizeof (float));
		buf[2] = (float*) malloc(w * h * sizeof (float));
		wavelet_denoise(buf, w, h, amt[i] * sv, low[i] * lv, DENOISE_LINEAR, "", 0.0f);
		free(buf[1]);
		free(buf[2]);
	}
	
	return img;
}

Image* CR2_profiled_denoise(Image* img, int iso, bool baseline_nr)
{
    if (iso >= 800)
    {
        const int32_t NR_ISO[] =    {400, 800, 1600, 3200, 6400, 12800, 25600, 51200};
        const float NR_STRENGTH[] = {0.05f, 0.08f, 0.115f, 0.15f, 0.175f, 0.225f, 0.28f, 0.35f};
        const float NR_LOW[] =      {0.85f, 0.8f, 0.7f, 0.61f, 0.53f, 0.45f, 0.4f, 0.37f};

        float sv = NR_STRENGTH[COUNT(NR_ISO) - 1];
        float lv = NR_LOW[COUNT(NR_ISO) - 1];

        for (uint32_t i = 1; i < COUNT(NR_ISO); i ++)
        {
            const int32_t h_iso = NR_ISO[i], l_iso = NR_ISO[i - 1];
            if (iso <= h_iso)
            {
                const float s = (float) (iso - l_iso) / (float) (h_iso - l_iso);
                sv = NR_STRENGTH[i - 1] * (1 - s) + NR_STRENGTH[i] * s;
                lv = NR_LOW[i - 1] * (1 - s) + NR_LOW[i] * s;
                break;
            }
        }

    	img = CR2_denoise(img, sv, lv);
    }
    else if (baseline_nr)
    {
	    img = CR2_denoise(img, 0.05f, 0.8f);
    }
    
    return img;
}

Image* CR2_colorTransform(Image* img, const float* from, const float* to, const float* fwb, const float* twb)
{
    printf("AHD Demosaic...\n");

    Image* flat = img->demosaicRAW(eImageDemosaic_AHD, from, fwb);
    delete img;

    printf("Color space conversion...\n");

    const int32_t w = flat->width(), h = flat->height();
    const int32_t s = w * h;
    
    float* ch[] = {flat->channel(0), flat->channel(1), flat->channel(2)};
    float inv_from[9], cam_cam[9];

    m3x3Inverse(inv_from, to);
    m3x3Multiply(cam_cam, from, inv_from);

	printf("Color matrix: ");
	FORC(9) printf("%f ", cam_cam[c]);
	printf("\n");
    
    float amax = 0.0f, amin = 0.0f;
    float* vmax;
    float* vmin;

#pragma omp parallel
    {
#pragma omp single
    {
        const int nth = omp_get_num_threads();
        vmax = new float[nth];
        vmin = new float[nth];

        for (int i = 0; i < nth; i ++)
        {
            vmax[i] = -1.0f;
            vmin[i] = 1.0f;
        }
    }

    const int ith = omp_get_thread_num();
    float &tmax = vmax[ith];
    float &tmin = vmin[ith];

#pragma omp for
    for (int32_t i = 0; i < s; i ++)
    {
        const float r = ch[0][i] * fwb[0];
        const float g = ch[1][i] * fwb[1];
        const float b = ch[2][i] * fwb[2];

        const float dr = (r * cam_cam[0] + g * cam_cam[3] + b * cam_cam[6]) / twb[0];
        const float dg = (r * cam_cam[1] + g * cam_cam[4] + b * cam_cam[7]) / twb[1];
        const float db = (r * cam_cam[2] + g * cam_cam[5] + b * cam_cam[8]) / twb[2];

        if (tmax < dr) tmax = dr;
        if (tmax < dg) tmax = dg;
        if (tmax < db) tmax = db;

        if (tmin > dr) tmin = dr;
        if (tmin > dg) tmin = dg;
        if (tmin > db) tmin = db;

        ch[0][i] = dr;
        ch[1][i] = dg;
        ch[2][i] = db;
    }
    
#pragma omp single
    {
        const int nth = omp_get_num_threads();

        for (int i = 0; i < nth; i ++)
        {
            if (amax < vmax[i]) amax = vmax[i];
            if (amin > vmin[i]) amin = vmin[i];
        }

        delete[] vmax;
        delete[] vmin;
    }
    }
    

    printf("Gamut range: %.4f, %.4f\n", amin, amax);
     
    // Scale r/g/b down to [0-1]
    if (amax > 1.0f)
    {
        printf("Gamut compression...\n");
        const float rmax = 1.0f / amax;

#pragma omp parallel for
        for (int32_t c = 0; c < 3; c ++)
        {
            float* ch = flat->channel(c);
            for (int32_t i = 0; i < s; i ++)
            {
                ch[i] = ch[i] * rmax;
            }
        }
    }
    
	Image* rebayer = flat->remosaicRAW();
	delete flat;
    
    return rebayer;
}

Image* CR2_profiled_color(Image* img, const CamInfo* from, const CamInfo* to, const float dlmix, const float green, const float rgreen)
{
    float mat_from_d50[9], mat_from_stda[9];
    float mat_to_d50[9], mat_to_stda[9];

    for (int32_t i = 0; i < 9; i ++)
    {
        mat_from_d50[i] = from->cam_d50[i];
        mat_from_stda[i] = from->cam_stda[i];
        mat_to_d50[i] = to->cam_d50[i];
        mat_to_stda[i] = to->cam_stda[i];
    }
    
    printf("Dual illuminant mixing: %.4f, Green: %.4f -> %.4f\n", dlmix, green, rgreen);
    float mat_from[9], mat_to[9];
    
    m3x3Interpolate(mat_from, mat_from_stda, mat_from_d50, dlmix);
    m3x3Interpolate(mat_to, mat_to_stda, mat_to_d50, dlmix);
    
    /*float delta_from =*/ m3x3ColorEqualize(mat_from);
    /*float delta_to =*/ m3x3ColorEqualize(mat_to);
    
    //printf("Color matrix interpolation error: %f, %f\n", delta_from, delta_to);
	
	const float idl = 1.0f - dlmix;
	float fwb[3], twb[3];
	
	FORC(3) fwb[c] = from->wb_d50[c] * dlmix + from->wb_stda[c] * idl;
	FORC(3) twb[c] = to->wb_d50[c] * dlmix + to->wb_stda[c] * idl;

    fwb[1] *= green;
    twb[1] *= rgreen;
	
    img = CR2_colorTransform(img, mat_from, mat_to, fwb, twb);
    
    return img;
}

/* Deprecated, since new color matrix does not have white point info */
void CR2_white_point(const CamInfo* camfrom, const CamInfo* camto, const float* from, float* to)
{
    float mat_d50[9];
    FORC(9) mat_d50[c] = camfrom->cam_d50[c];
    float mat_to_d50[9];
    FORC(9) mat_to_d50[c] = camto->cam_d50[c];

    const float sr = mat_d50[0] + mat_d50[1] + mat_d50[2];
    const float sg = mat_d50[3] + mat_d50[4] + mat_d50[5];
    const float sb = mat_d50[6] + mat_d50[7] + mat_d50[8];
    //printf("From white point: %.4f, %.4f, %.4f\n", sr, sg, sb);

    const float tr = mat_to_d50[0] + mat_to_d50[1] + mat_to_d50[2];
    const float tg = mat_to_d50[3] + mat_to_d50[4] + mat_to_d50[5];
    const float tb = mat_to_d50[6] + mat_to_d50[7] + mat_to_d50[8];
    //printf("To white point: %.4f, %.4f, %.4f\n", tr, tg, tb);

    to[0] = (float) from[0] / (float) from[1] * (sr / tr);
    to[1] = (sg / tg);
    to[2] = (float) from[2] / (float) from[1] * (sb / tb);
    //printf("WB: %.4f, %.4f, %.4f\n", to[0], to[1], to[2]);
}
