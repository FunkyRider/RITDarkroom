#include "os.h"

#include <math.h>
#include <memory.h>
#include "cr2raw.h"
#include "cr2conv.h"

#include "cr2plugins.h"
#include "image.h"
#include "denoise.h"
#include "resample.h"

#include "ptr.h"
#include "array.h"
#include "tile.h"

//#include <omp.h>
#include "cr2plugins.h"
#include "ustring.h"
#include "crzops.h"

Array<LensInfo*>	g_lensInfo;
Array<CamInfo*>		g_camInfo;

static int g_TemplateId = 0;
static bool g_Convert = true;

const CamInfo _template_cam[] = {
    {"1DS2_BLK",			1, 0, 129, 3924, {2.24f, 1.0f, 1.32f}, {1.19f, 1.0f, 2.71f}, {1.99f, -0.16f, 0.02f, -1.06f, 1.46f, -0.37f, 0.07f, -0.3f, 1.35f}, {1.89f, -0.18f, 0.09f, -0.88f, 1.35f, -0.62f, -0.01f, -0.17f, 1.53f}}
};

static const char CDATA_HEADER[8] = {'A', 'S', 'C', 'I', 'I', 0, 0, 0};

void CR2_InitProfile()
{
    CR2_LoadCamInfo("cam_canon.txt", g_camInfo);
    CR2_LoadCamInfo("cam_pana.txt", g_camInfo);
    CR2_LoadLensInfo("lens_canon.txt", g_lensInfo);
    CR2_LoadLensInfo("lens_m43.txt", g_lensInfo);
}

bool CR2_IsConvert()
{
	return g_Convert;
}

const CamInfo& CR2_GetTemplateCam()
{
	return _template_cam[g_TemplateId];
}

String Underscore_Lens_Name(const char* name)
{
	String ret(name);
	const size_t l = ret.length();
	
	for (size_t i = 0; i < l; i ++)
	{
		if (ret[i] == ' ') ret[i] = '_';
	}
	
	return ret;
}

float g_From_RGB(float r, float g, float b)
{
	if (fabs(r - g) > fabs(b - g))
	{
		float r2 = (r - g) / 2.0f + g;
		float g2 = ((r2 + b) / 2.0f) / g;
		return g2;
	}
	else
	{
		float b2 = (b - g) / 2.0f + g;
		float g2 = ((r + b2) / 2.0f) / g;
		return g2;
	}
}

int CR2_Convert_Canon(CR2C_Info* _info, CR2Raw* dest, CR2Raw* src, CamInfo* info, const char* dfile, bool fluorscent_light)
{
    bool fullSize = _template_cam[g_TemplateId].nr ? true : false;
	bool convert = (info->cam_d50[0] > 0) & g_Convert;
	const int32_t dw = dest->getWidth(), dh = dest->getHeight();
    uint16_t* ddata = dest->getRawData();

    int32_t _black = 0, _saturation = 4095;
    CR2_GetCamBlackSaturation(*info, dest->getISO(), _black, _saturation);	
    char _szcblk[12] = {0};

    if (_black == 0)
    {
        _black = CR2_CalculateBlack(dest);
        strcpy(_szcblk, " (auto)");        
    }
    printf("Black: %d%s, Saturation: %d\n", _black, _szcblk, _saturation);

    const int32_t sw = convert ? src->getWidth() : dw, sh = convert ? src->getHeight() : dh;
	const uint16_t blk = _template_cam[g_TemplateId].black;
    uint16_t* sdata = convert ? (fullSize ? src->createRawData(dw, dh, blk) : src->createRawData(sw, sh, blk)) : NULL;

	uint16_t* tail = ddata + dw * (dh - 1);
	for (int i = 0; i < dw; i ++)
	{
		tail[i] = blk;
	}

    if (convert)
	    src->copyExif(dest);
    
    CR2_removeHotPixels(dest);
    CR2_ADCColumnBias(dest);
    
    const bool sgbgb = (_template_cam[g_TemplateId].gbgb == 1);
    const bool gbgb = (info->gbgb == 1);
	const int32_t bh = (gbgb) ? dh - 2 : dh;
    Image* bayer = NULL;

    // Convert to Image object
	{
    	// Some RAW data starts with GBGB instead of RGRG, skip first line so demosaic algorithm can work.
		bayer = new Image(dw / 2, bh / 2, eImageFormat_RGBG);
		bayer->fromRAW16((gbgb) ? ddata + dw : ddata, _saturation, _black);
	}

	int lindex = -1;
	if (_info->ca_correct)
	{
    	bayer = CR2_profiled_lens(bayer, dest, g_lensInfo, lindex);
    }
    else
    {
    	lindex = CR2_get_lens_index(dest, g_lensInfo);
    }

    if (_info->raw_nr)
    {
    	bayer = CR2_profiled_denoise(bayer, dest->getISO(), (info->nr > 0));
    }
    
    // Dual-Illuminant mixing factor
    float dlmix = 1.0f, green = 1.0f;
    
    if (_info->color_mat)
    {    
		// Color space transform
		if (convert)
		{
			uint32_t *swb = dest->getWBCoefficients();
			float wb1 = info->wb_d50[0] / info->wb_d50[2];
			float wb2 = info->wb_stda[0] / info->wb_stda[2];
			float awb = (float) swb[0] / (float) swb[3];			
			dlmix = (awb - wb2) / (wb1 - wb2);
		    if (dlmix < 0) dlmix = 0; else if (dlmix > 1) dlmix = 1;
            
            // Calculate green
			/*
            float g1 = g_From_RGB(info->wb_d50[0], info->wb_d50[1], info->wb_d50[2]);
            float g2 = g_From_RGB(info->wb_stda[0], info->wb_stda[1], info->wb_stda[2]);
            float gmix = g1 * dlmix + g2 * (1.0f - dlmix);
			float ga = g_From_RGB((float) swb[0] / (float) swb[1], 1.0f, (float) swb[3] / (float) swb[1]);
            float green = ga / gmix;
            if (green < 0.5f) green = 0.5f; else if (green > 2.0f) green = 2.0f;


			float g_a = g_From_RGB(swb[8], swb[9], swb[11]);
			float g_b = g_From_RGB(swb[12], swb[13], swb[15]);
			float g_c = g_From_RGB(swb[16], swb[17], swb[19]);
			float g_d = g_From_RGB(swb[20], swb[21], swb[23]);
			float g_e = g_From_RGB(swb[24], swb[25], swb[27]);


			float rg1 = g_From_RGB(_template_cam[g_TemplateId].wb_d50[0], _template_cam[g_TemplateId].wb_d50[1], _template_cam[g_TemplateId].wb_d50[2]);
			float rg2 = g_From_RGB(_template_cam[g_TemplateId].wb_stda[0], _template_cam[g_TemplateId].wb_stda[1], _template_cam[g_TemplateId].wb_stda[2]);
            float rgmix = rg1 * dlmix + rg2 * (1.0f - dlmix);
			*/

		    bayer = CR2_profiled_color(bayer, info, &_template_cam[g_TemplateId], dlmix, fluorscent_light ? 0.93f : 1.0f, fluorscent_light ? 0.93f : 1.0f);
		}

		// WB as shot conversion, CR2-CR2 specific
		if (convert)
		{
			float fwb[3], twb[3];
			const float idl = 1.0f - dlmix;
		    FORC(3) fwb[c] = info->wb_d50[c] * dlmix + info->wb_stda[c] * idl;
			FORC(3) twb[c] = _template_cam[g_TemplateId].wb_d50[c] * dlmix + _template_cam[g_TemplateId].wb_stda[c] * idl;
		
		    uint32_t *swb = src->getWBCoefficients();
		    uint32_t *dwb = dest->getWBCoefficients();
		    
			float f[3] = {1, 1, 1};		
		    printf("Original AWB: %d %d %d %d\n", swb[0], swb[1], swb[2], swb[3]);
		    
		    f[0] = twb[0] / fwb[0];
		    f[2] = twb[2] / fwb[2];
		    printf("WB scales: %f %f %f\n", f[0], f[1], f[2]);
		    
		    for (int i = 0; i < 5; i ++)
		    {
		    	//printf("swb: [%d k] %d %d %d, dwb: [%d k] %d %d %d\n", stemp[i], swb[0 + i * 4], swb[1 + i * 4], swb[3 + i * 4], dtemp[i], dwb[0 + i * 4], dwb[1 + i * 4], dwb[3 + i * 4]);
				swb[0 + i * 4] = (uint32_t)((float) dwb[0 + i * 4] * f[0]);
				swb[1 + i * 4] = (uint32_t)((float) dwb[1 + i * 4]);
				swb[3 + i * 4] = (uint32_t)((float) dwb[3 + i * 4] * f[2]);
				swb[2 + i * 4] = swb[1 + i * 4];
		    	//printf("* swb: [%d k] %d %d %d, dwb: [%d k] %d %d %d\n", stemp[i], swb[0 + i * 4], swb[1 + i * 4], swb[3 + i * 4], dtemp[i], dwb[0 + i * 4], dwb[1 + i * 4], dwb[3 + i * 4]);
		    }
		    
		    printf("Scaled AWB: %d %d %d %d\n", swb[0], swb[1], swb[2], swb[3]);
		}
	}
	
	// Convert back from Image object
	{
		bayer->toRAW16((gbgb) ? ddata + dw : ddata, convert ? _template_cam[g_TemplateId].saturation : _saturation, convert ? blk : _black);
		delete bayer;
	}
	
	// Copy RAW data
    if (convert && fullSize)
	{
        if (gbgb != sgbgb)
        {
            memcpy(sdata, ddata + dw, dw * (dh - 1) * 2);
        }
        else
        {
		    memcpy(sdata, ddata, dw * dh * 2);
        }

		uint16_t *s = src->getSliceData();		
        s[2] = (s[0] == 0 && s[1] == 0) ? 0 : dw - s[0] * s[1];
		memcpy(src->getSensorInfo(), dest->getSensorInfo(), 17 * 2);
	}
	
	int dleft = 0, dtop = 0;
	int dw_real = 0, dh_real = 0;
	
    // Crop RAW data
    if (convert && !fullSize)
    {        
        const uint16_t* ssen = src->getSensorInfo();    // 1Ds2/3
        const uint16_t* dsen = dest->getSensorInfo();   // Target

        const uint16_t _cam0[] = {ssen[5], ssen[6], ssen[7] - ssen[5] + 1, ssen[8] - ssen[6] + 1};
        const uint16_t _cam1[] = {dsen[5], dsen[6], dsen[7] - dsen[5] + 1, dsen[8] - dsen[6] + 1};

	    const int yadjust = ((gbgb != sgbgb) ^ ((_cam0[1] & 1) != (_cam1[1] & 1))) ? 1 : 0;
	    
	    dtop = yadjust;
	    //printf("yadjust: %d\n", yadjust);

        if (_cam1[2] > _cam0[2] && _cam1[3] > _cam0[3])
        {
            // Tiled
            rect_t rgn[8], fit[8];
            
            // Save borders first, then use default code to save center
            const int c = Tile_Split(_cam1[2] + 8, _cam1[3] + 8, _cam0[2] - 8, _cam0[3] - 8, rgn, fit, 8);

            if (c > 0)
            {
                for (int i = 1; i < c; i ++)
                {
                    rect_t srgn;
                    srgn.assign(rgn[i]);
                    srgn.adjust(_cam1[0] - 4, _cam1[1] - 4, 0, 0);
                    Tile_Blit(sdata, sw, sh, fit[i].x + _cam0[0], fit[i].y + _cam0[1] + yadjust, ddata, dw, dh, srgn);
                }

                char buf[512];
                strcpy(buf, dfile);
                size_t blen = strlen(buf);
                buf[blen - 1] = '_';
                
		        printf("Saving split part: %s\n", buf);

                CRZ_SaveStream(src, buf);

                rect_t srgn;
                srgn.assign(rgn[0]);
                srgn.adjust(_cam1[0] - 4, _cam1[1] - 4, 0, 0);
                Tile_Blit(sdata, sw, sh, fit[0].x + _cam0[0], fit[0].y + _cam0[1] + yadjust, ddata, dw, dh, srgn);
            }
            
            dw_real = _cam1[2] + 8;
            dh_real = _cam1[3] + 8;
        }
        else
        {
            // Cropped
		    const int32_t diff_w = _cam1[2] - _cam0[2],
				    diff_h = _cam1[3] - _cam0[3];
		    const int32_t diff_x = diff_w / 2, diff_y = diff_h / 2;	

		    int32_t xstart, xwidth, ystart, ywidth, xsrc, ysrc;
	
		    if (diff_w > 0)
		    {
			    // Crop
			    xstart = (_cam0[0] >> 1) << 1;
			    xwidth = _cam0[2];
			    xsrc = _cam1[0] + diff_x;
		    }
		    else
		    {
			    // Center
			    xstart = _cam0[0] - diff_x;
			    xwidth = _cam1[2];
			    xsrc = _cam1[0];		
			    xstart = (xstart >> 1) << 1;
		    }

            if (diff_h > 0)
            {
			    ystart = (_cam0[1] >> 1) << 1;
			    ywidth = _cam0[3];
			    ysrc = _cam1[1] + diff_y;
            }
            else
            {
			    ystart = _cam0[1] - diff_y;
			    ywidth = _cam1[3];
			    ysrc = _cam1[1];
                ystart = (ystart >> 1) << 1;
            }

            //ystart += yadjust;
        	//if (diff_h > 0) ywidth -= yadjust;
            
            dtop = ystart - _cam0[1] - 4;
    		dleft = xstart - _cam0[0] - 4;
            if (dtop < 0) dtop = 0;
            if (dleft < 0) dleft = 0;
		    dw_real = xwidth + 8;
		    dh_real = ywidth + 8;

            for (int32_t y = -4; y < ywidth; y ++)
		    {
			    uint16_t* s = sdata + (y + ystart) * sw + xstart;
			    uint16_t* d = ddata + (y + ysrc) * dw + xsrc;
		        memcpy(s - 4, d - 4, xwidth * 2 + 16);
		    }
        }
    }
    
    char* cdata = (convert) ? src->getComment() : dest->getComment();
	memcpy(cdata, CDATA_HEADER, 8);

    char lensdata[128] = {0};
	if (lindex >= 0)
	{
		int lid = g_lensInfo[lindex]->id;
			
		if (lid > 0 && lid < 65535)
		{		
			sprintf(lensdata, " LID:%d", lid);
		}
		else
		{
			sprintf(lensdata, " LENS:%s", Underscore_Lens_Name(g_lensInfo[lindex]->dispName).c_str());	
		}
	}
	else if (src->getLensModel()[0] != 0)
	{
		sprintf(lensdata, " LENS:%s", Underscore_Lens_Name(src->getLensModel()).c_str());
	}

    // Crop data and lens model
    if (convert)
    {		
		sprintf(cdata + 8, "VER:1.3 POS:%d,%d,%d,%d%s", dleft, dtop, dw_real, dh_real, lensdata);
	}
    else
    {
        sprintf(cdata + 8, "VER:1.3%s", lensdata);
    }
	
	// Save output
	{
		printf("Saving: %s\n", dfile);

        if (FILE* fp = _fopen(dfile, "rb"))
        {
            fclose(fp);
            src->loadVRD(dfile);
        }

        if (convert)
        {
    		src->save(dfile, dest);
        }
        else
        {
            dest->save(dfile);
        }
	}
    
    return 0;
}

int CR2_Convert_Pana(CR2C_Info* _info, CR2Raw* dest, CR2Raw* src, CamInfo* info, const char* dfile, bool fluorscent_light)
{
	const int32_t dw = dest->getWidth();//, dh = dest->getHeight();
    uint16_t* ddata = dest->getRawData();
    const uint32_t* blksatwb = dest->getRW2BlackSatWB();
    
    const int32_t sw = src->getWidth(), sh = src->getHeight();
    uint16_t* sdata = src->createRawData(sw, sh, blksatwb[0]);

    printf("Black: %d, Saturation: %d\n", blksatwb[0], blksatwb[3]);

	int dleft = 0, dtop = 0;
	int dw_real = 0, dh_real = 0;
	
	// Copy RAW data
	{
        uint16_t* ssen = src->getSensorInfo();
        uint16_t* dsen = dest->getSensorInfo();
        
        uint16_t _cam0[] = {ssen[5], ssen[6], ssen[7] - ssen[5] + 1, ssen[8] - ssen[6] + 1};
        uint16_t _cam1[] = {dsen[5], dsen[6], dsen[7] - dsen[5] + 1, dsen[8] - dsen[6] + 1};
		int32_t diff_w = _cam1[2] - _cam0[2],
				diff_h = _cam1[3] - _cam0[3];
		int32_t diff_x = diff_w / 2, diff_y = diff_h / 2;	
		int32_t xstart, xwidth, ystart, ywidth, xsrc, ysrc;
	
		if (diff_w > 0)
		{
			// Crop
			xstart = (_cam0[0] >> 1) << 1;
			xwidth = _cam0[2];
			xsrc = _cam1[0] + diff_x;
		}
		else
		{
			// Center
			xstart = _cam0[0] - diff_x;
			xwidth = _cam1[2];
			xsrc = _cam1[0];		
			xstart = (xstart >> 1) << 1;
		}

        if (diff_h > 0)
        {
			ystart = (_cam0[1] >> 1) << 1;
			ywidth = _cam0[3];
			ysrc = _cam1[1] + diff_y;
        }
        else
        {
			ystart = _cam0[1] - diff_y;
			ywidth = _cam1[3];
			ysrc = _cam1[1];
            ystart = (ystart >> 1) << 1;
        }

		int filters = dest->getCfaType();
		filters = 0x01010101 * (uint8_t) "\x94\x61\x49\x16"
		[((filters - 1) ^ (dsen[5] & 1) ^ (dsen[6] << 1)) & 3];
        
        printf("CFA Pattern: %08X\n", filters);

        
        switch (filters)
        {
        case 0x61616161:	// GB/RG                // good
        	xstart ++;
        	if (diff_w > 0) xwidth --;        	
        	ystart ++;
        	if (diff_h > 0) ywidth --;
        	break;
        case 0x94949494:	// BG/GR
        	ystart ++;
        	if (diff_h > 0) ywidth --;
        	break;
        case 0x16161616:	// RG/GB
        	xstart ++;
        	if (diff_w > 0) xwidth --;
        	break;
        case 0x49494949:	// GR/BG, 1Ds2 native   // good
        default:
        	break;
        }

		dtop = ystart - _cam0[1];
		dleft = xstart - _cam0[0];
        if (dtop < 0) dtop = 0;
        if (dleft < 0) dleft = 0;
		dh_real = ywidth;
		dw_real = xwidth;
		
		dsen[5] = xstart;
		dsen[6] = ystart;
		dsen[7] = xstart + xwidth;
		dsen[8] = ystart + ywidth;

		for (int32_t y = 0; y < ywidth; y ++)
		{
			uint16_t* s = sdata + (y + ystart) * sw + xstart;
			uint16_t* d = ddata + (y + ysrc) * dw + xsrc;
		    memcpy(s, d, xwidth * 2);
		}
	}
	
	src->copyExif(dest);
	
	// From uint16
    Image* bayer = new Image(sw / 2, sh / 2, eImageFormat_RGBG);
	bayer->fromRAW16(sdata + sw, blksatwb[3], blksatwb[0]);

	int lindex = -1;
	if (_info->ca_correct)
	{
    	bayer = CR2_profiled_lens(bayer, dest, g_lensInfo, lindex);
    }
    else
    {
    	lindex = CR2_get_lens_index(dest, g_lensInfo);
    }

	if (_info->raw_nr)
	{
		bayer = CR2_profiled_denoise(bayer, dest->getISO(), (info->nr > 0));
    }
    
    // Dual-Illuminant mixing factor
    float dlmix = 1.0f;
        
    if (_info->color_mat)
    {
		// Color space transform
		{
			float wb1 = info->wb_d50[0] / info->wb_d50[2];
			float wb2 = info->wb_stda[0] / info->wb_stda[2];
			float awb = (float) blksatwb[6] / (float) blksatwb[8];
			dlmix = (awb - wb2) / (wb1 - wb2);
		    if (dlmix < 0) dlmix = 0; else if (dlmix > 1) dlmix = 1;
		    
            // Calculate green
			/*
            float g1 = g_From_RGB(info->wb_d50[0], info->wb_d50[1], info->wb_d50[2]);
            float g2 = g_From_RGB(info->wb_stda[0], info->wb_stda[1], info->wb_stda[2]);
            float gmix = g1 * dlmix + g2 * (1.0f - dlmix);
            float ga = g_From_RGB((float) blksatwb[6] / (float) blksatwb[7], 1.0f, (float) blksatwb[8] / (float) blksatwb[7]);
            float green = ga / gmix;
            if (green < 0.5f) green = 0.5f; else if (green > 2.0f) green = 2.0f;
			*/

		    bayer = CR2_profiled_color(bayer, info, &_template_cam[g_TemplateId], dlmix, fluorscent_light ? 0.93f : 1.0f, fluorscent_light ? 0.93f : 1.0f);
		}

		// WB as shot translation
		{
		    float to[3] = {(float) blksatwb[6], (float) blksatwb[7], (float) blksatwb[8]};        
		    
		    float fwb[3], twb[3];
		    const float idl = 1.0f - dlmix;
		    FORC(3) fwb[c] = info->wb_d50[c] * dlmix + info->wb_stda[c] * idl;
			FORC(3) twb[c] = _template_cam[g_TemplateId].wb_d50[c] * dlmix + _template_cam[g_TemplateId].wb_stda[c] * idl;
		
			float f[3] = {1, 1, 1};
		    f[0] = twb[0] / fwb[0];
		    f[2] = twb[2] / fwb[2];
		
			to[0] *= f[0];
			to[2] *= f[2];
		
		    uint32_t* wb = src->getWBCoefficients();
		    wb[0] = (uint16_t) (1024.0f * (to[0] / to[1]));
		    wb[1] = 1024;
		    wb[2] = 1024;
		    wb[3] = (uint16_t) (1024.0f * (to[2] / to[1]));
		    
		    printf("Scaled AWB: %d %d %d %d\n", wb[0], wb[1], wb[2], wb[3]);
		}
	}
	
    // To uint16
    bayer->toRAW16(sdata + sw, _template_cam[g_TemplateId].saturation, _template_cam[g_TemplateId].black);
    delete bayer;

	// Crop data and lens model
	{
		char* cdata = src->getComment();
		memcpy(cdata, CDATA_HEADER, 8);

		char lensdata[128] = {0};
		if (lindex >= 0)
		{
			int lid = g_lensInfo[lindex]->id;
			
			if (lid > 0 && lid < 65535)
			{
				sprintf(lensdata, " LID:%d", lid);
			}
			else
			{
				sprintf(lensdata, " LENS:%s", Underscore_Lens_Name(g_lensInfo[lindex]->dispName).c_str());	
			}
		}
		else if (src->getLensModel()[0] != 0)
		{
			sprintf(lensdata, " LENS:%s", Underscore_Lens_Name(src->getLensModel()).c_str());
		}
		
		char brand[16] = {0};
		
		if (info->model[0] == 'D' && info->model[1] == 'M' && info->model[2] == 'C' && info->model[3] == '-')
		{
			strcpy(brand, " BRD:Panasonic");
		}
		else if (info->model[1] == '-' && info->model[2] == 'L' && info->model[3] == 'U' && info->model[4] == 'X')
		{
			strcpy(brand, " BRD:LEICA");
		}
	
		sprintf(cdata + 8, "VER:1.3 POS:%d,%d,%d,%d%s%s", dleft, dtop, dw_real, dh_real, brand, lensdata);
	}
	
	// Save output
    {
		printf("Saving: %s\n", dfile);

        if (FILE* fp = _fopen(dfile, "rb"))
        {
            fclose(fp);
            src->loadVRD(dfile);
        }

        src->save(dfile, dest);
    }
    
    return 0;
}

int CR2_Convert(CR2C_Info* info, const char* fsrc, const char* fdst, bool fluorscent_light)
{
	int res = 0;

	try {
		printf("Loading: %s\n", fsrc);

		CR2Raw dest(fsrc);	
		printf("Camera: %s\n", dest.getModel());
		printf("Serial Number: %s\n", dest.getSerialNumber());
	
		int id = -1;	
		for (uint32_t i = 0; i < g_camInfo.size(); i ++)
		{
			if (strcmp(g_camInfo[i]->model, dest.getModel()) == 0)
			{
				id = i;
				break;
			}
		}
	
		if (id < 0)
		{
			printf("Camera is not supported. Skipping...\n");
			return 0;
		}

		size_t clen = dest.getCommentLen();
		if (clen > 0)
		{	
			String cdata(dest.getComment() + 8);
			size_t p = 0;		
			String var = cdata.term(p);

			while (var.length() > 0)
			{
				size_t vp = 0;
				String name = var.term(vp, ':');
				String val = var.term(vp);
			
				if (name == "VER")
				{
					printf("Image is already imported. Skipping...\n");
					return 0;
				}
			}
		}

		bool convert = (g_camInfo[id]->cam_d50[0] > 0) & CR2_IsConvert();
		char path[512];
		strcpy(path, getAppPath());
		strcat(path, CR2_GetTemplateCam().model);
		CR2Raw* src = convert ? new CR2Raw(path) : NULL;
		    
		char dfile[512];	
		strcpy(dfile, fsrc);
		size_t l = strlen(dfile);
		dfile[l - 4] = '\0';
		strcat(dfile, "_RIT.CR2");

		if (fdst)
		{
			strcpy(dfile, fdst);
		}
	
		switch (dest.getRawFormat())
		{
		case CR2RF_CanonCR2:
			if (CR2_Convert_Canon(info, &dest, src, g_camInfo[id], dfile, fluorscent_light) == 0)
				res = 1;
			break;
		case CR2RF_PanasonicRW2:
			if (CR2_Convert_Pana(info, &dest, src, g_camInfo[id], dfile, fluorscent_light) == 0)
				res = 1;
			break;
		default:
			printf("Error: Unknown RAW format!\n");
			break;
		}
	
		delete src;
	}
    catch (CR2Exception e)
    {
        e.print();

        return -1;
    }
    
    return res;
}

