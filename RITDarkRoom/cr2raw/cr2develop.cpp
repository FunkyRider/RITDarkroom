#include "os.h"
#include <math.h>
#include <memory.h>
#include "image.h"
#include "cr2raw.h"
#include "denoise.h"
#include "sharpen.h"
#include "tile.h"
#include "saveimage.h"
#include "cr2develop.h"
#include "exif.h"
#include "cr2plugins.h"
//#include <omp.h>
#include "crzops.h"

#ifdef WIN32
#include "../cr2info/cr2info.h"
#else
#include "cr2client.h"
#define CR2Info CR2Client
#endif

#include "ustring.h"

extern Array<LensInfo*>		g_lensInfo;
extern Array<CamInfo*>		g_camInfo;

Image* CR2_Develop(CR2Info &cr2, const char* file)
{
	string path(file);
    cr2.open(path);

    const int sw = cr2.width(), sh = cr2.height();
    uint16_t* raw = new uint16_t[sw * sh * 3];
    printf("Developing %s\n", path.c_str());
    cr2.getFull((void*) raw);
    cr2.close();
    Image* img1 = new Image(sw, sh, eImageFormat_RGB);
    img1->fromRGB16(raw);
    delete[] raw;
    
    return img1;
}

Image* CR2_DevelopSplit(CR2Info* cr2, CR2Info* cr2_s, const char* file, int dw, int dh)
{
    char tmp_cr2[512];
    getTempFile(tmp_cr2, 512);

	CR2Info* driver[2] = {cr2, cr2_s};
	const char* path[2] = {file, tmp_cr2};
    string patht(file);
	Image* img[2] = {NULL, NULL};
	
    patht[patht.length() - 1] = '_';
    
    if (CRZ_CreateCR2(file, patht.c_str(), tmp_cr2) < 0)
    {
        deleteFile(tmp_cr2);
        return NULL;
    }

	int sw = 0, sh = 0;

#pragma omp parallel
    {
#pragma omp single
    {
        const int nth = omp_get_num_threads();
		int th = omp_get_num_threads() < 2 ? 1 : 2;
		printf("Developing %s (%d threads)\n", file, th);
	}
#pragma omp for
	for (int i = 0; i < 2; i ++)
	{
		driver[i]->open(path[i]);
        //printf("%s\n", driver[i]->getAdjustments().c_str());
		int _sw = driver[i]->width(), _sh = driver[i]->height();
		if (i == 0)
		{
			sw = _sw;
			sh = _sh;
		}
		uint16_t* raw = new uint16_t[_sw * _sh * 3];
		driver[i]->getFull((void*) raw);
		driver[i]->close();
		img[i] = new Image(_sw, _sh, eImageFormat_RGB);
		img[i]->fromRGB16(raw);
		delete[] raw;
	}
	}

    rect_t rgn[8], fit[8];
    int ts = Tile_Combine(dw, dh, sw - 8, sh - 8, rgn, fit, 8);

    Image* ret = new Image(dw, dh, eImageFormat_RGB);

    printf("Combining results\n");

    for (int s = 0; s < ts; s ++)
    {
        for (int c = 0; c < 3; c ++)
        {
            Image* src = (s == 0) ? img[0] : img[1];
            src->blit(ret, c, c, fit[s].x, fit[s].y + 3, rgn[s].w, rgn[s].h, rgn[s].x, rgn[s].y);
        }
    }

	for (int i = 0; i < 2; i ++)
	{
		delete img[i];
	}
    
    deleteFile(path[1]);
    
    return ret;
}


int saveImage(const char* fout, Image &img, bool tiff, const ExifIfd* exif)
{
	uint32_t w = img.width(), h = img.height();
	
	if (tiff)
    {
		uint16_t* buf = (uint16_t*) malloc(w * h * 6);
	    img.toFormat(eImageFormat_RGB);
	    img.toRGB16(buf);
	    printf("Saving 16-bit TIFF: %s\n", fout);
	    SaveTIFF2(buf, w, h, 48, fout, exif);
	    free(buf);
    }
    else
    {
		uint8_t* buf = (uint8_t*) malloc(w * h * 3);        
	    img.toFormat(eImageFormat_RGB);
	    img.toRGB8(buf);
	    printf("Saving JPG: %s\n", fout);
	    SaveJPEG2(buf, w, h, 24, fout, 100, exif);
	    free(buf);
    }
    
    return 0;
}

const float DENOISE_SIZE = 8;
float DENOISE_AMT[][8] = {
    {200, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 4096},
    {400, 0.12f, 0.74f, 0.8f, 0.2f, 0.0f, 4096},
    {800, 0.18f, 0.68f, 1.0f, 0.15f, 0.1f, 2048},
    {1600, 0.24f, 0.6f, 1.3f, 0.12f, 0.2f, 1024},
    {3200, 0.40f, 0.55f, 1.8f, 0.10f, 0.3f, 768},
    {6400, 0.65f, 0.45f, 2.7f, 0.07f, 0.5f, 512},
    {12800, 0.85f, 0.35f, 5.2f, 0.03f, 0.7f, 512},
    {65535, 0.85f, 0.35f, 6.2f, 0.03f, 0.7f, 512}
};

const float SHARPEN_SIZE = 8;
float SHARPEN_AMT[][8] = {
	{100, 4, 1, 0.1f, -0.02f, 24.0f},
	{200, 4, 1, 0.1f, -0.01f, 24.0f},
	{400, 4, 1, 0.15f, 0.03f, 24.0f},
	{800, 4, 1, 0.2f, 0.04f, 28.0f},
	{1600, 3, 1, 0.25f, 0.05f, 32.0f},
	{3200, 2, 1, 0.3f, 0.06f, 40.0f},
	{6400, 1, 1, 0.35f, 0.08f, 48.0f},
	{12800, 0, 1, 0.45f, 0.1f, 48.0f}
};

Image* CR2_PostProcess(CR2D_Info* info, Image* img, const char* file)
{
	CR2Raw rawsrc(file);
	
	int iso = rawsrc.getISO();
	
	float nr_yamt = 0, nr_ydet = 0, nr_camt = 0, nr_cdet = 0, nr_cnt = 0, nr_hth = 65535;
	for (int i = 0; i < DENOISE_SIZE; i ++)
	{
		if (iso <= DENOISE_AMT[i][0])
		{		
			const int ip = (i == 0) ? 0 : i - 1;
			const float fp = (i == 0) ? 1 : (((float) iso - DENOISE_AMT[ip][0]) / (DENOISE_AMT[i][0] - DENOISE_AMT[ip][0]));
			const float fpp = 1.0f - fp;
			
			nr_yamt = DENOISE_AMT[i][1] * fp + DENOISE_AMT[ip][1] * fpp;
			nr_ydet = DENOISE_AMT[i][2] * fp + DENOISE_AMT[ip][2] * fpp;
			nr_camt = DENOISE_AMT[i][3] * fp + DENOISE_AMT[ip][3] * fpp;
			nr_cdet = DENOISE_AMT[i][4] * fp + DENOISE_AMT[ip][4] * fpp;
			nr_cnt = DENOISE_AMT[i][5] * fp + DENOISE_AMT[ip][5] * fpp;			
			nr_hth = DENOISE_AMT[i][6] * fp + DENOISE_AMT[ip][6] * fpp;

			break;
		}
	}
	
	if (!info->luma_nr)
    {
    	nr_yamt = 0;
    	nr_ydet = 0;
    }
    
    if (!info->chroma_nr)
    {
    	nr_camt = 0;
    }

	if ((nr_yamt > 0 || nr_camt > 0))
	{
		img->toFormat(eImageFormat_YCbCr);
	
		printf("Noise reduction (y: %.2f, yd: %.2f, c: %.2f, cd: %.2f)...\n", nr_yamt, nr_ydet, nr_camt, nr_cdet);
		float imp = sqrtf(nr_hth) / 255.0f;
        Image* res = Denoise_Impulse(img, 0, imp * imp);
        if (img != res) { delete img; img = res; }

        res = Denoise_Wavelet(img, nr_yamt, nr_ydet, nr_camt, nr_cdet, nr_cnt);
        if (img != res) { delete img; img = res; }
	}	
        
	return img;
}

void Remove_Underscore_Lens_Name(String &ret)
{
	const size_t l = ret.length();
	
	for (size_t i = 0; i < l; i ++)
	{
		if (ret[i] == '_') ret[i] = ' ';
	}
}

int Find_Lens_Index(int lid, Array<LensInfo*> &data)
{
	const size_t l = data.size();

	for (size_t i = 0; i < l; i ++)
	{
		if (data[i]->id == lid)
			return i;
	}
	
	return -1;
}

int CR2_Develop(CR2D_Info* info, const char* fsrc, const char* fdst)
{
	CR2Raw rawsrc(fsrc);
	size_t clen = rawsrc.getCommentLen();
	uint16_t* sarea = rawsrc.getSensorInfo();
	rect_t cropRect, sensorRect;
	String lensName, camBrand;
	int vMaj = 0, vMin = 0;
	
	sensorRect.set(0, 0, sarea[7] - sarea[5] + 1, sarea[8] - sarea[6] + 1);
	cropRect.assign(sensorRect);
	
	if (clen > 0)
	{	
		String cdata(rawsrc.getComment() + 8);
		//printf("Comment text: %s\n", cdata.c_str());
		
		size_t p = 0;		
		String var = cdata.term(p);
		
		while (var.length() > 0)
		{
			size_t vp = 0;
			String name = var.term(vp, ':');
			String val = var.term(vp);
			
			if (name == "VER")
			{
				vp = 0;
				vMaj = val.term(vp, '.').toInt();
				vMin = val.term(vp).toInt();
			}
			else if (name == "POS")
			{
				vp = 0;
				cropRect.x = val.term(vp, ',').toInt();
				cropRect.y = val.term(vp, ',').toInt();
				cropRect.w = val.term(vp, ',').toInt();
				cropRect.h = val.term(vp, ',').toInt();
			}
			else if (name == "LID")
			{
				vp = 0;
				int lid = val.term(vp).toInt();				
				int index = Find_Lens_Index(lid, g_lensInfo);
				
				//printf("LID: %d, Index: %d\n", lid, index);
				if (index >= 0)
				{
					lensName = g_lensInfo[index]->dispName;
					if (lensName.length() == 0)
						lensName = g_lensInfo[index]->model;
				}
			}
			else if (name == "LENS")
			{
				lensName = val;
				Remove_Underscore_Lens_Name(lensName);
			}
			else if (name == "BRD")
			{
				camBrand = val;
				Remove_Underscore_Lens_Name(camBrand);
			}
					
			var = cdata.term(p);
		}		
	}

	printf("Crop area: %d, %d, %d, %d\n", cropRect.x, cropRect.y, cropRect.w, cropRect.h);
	//printf("Lens Model: %s\n", lensName.c_str());
	printf("RIT Darkroom CR2 version: %d.%d\n", vMaj, vMin);
	
	if (vMaj == 0 && vMin == 0)
	{
		printf("Error: CR2 not generated by RIT Darkroom, skipping...\n");
		return 0;
	}
	else if (vMaj > 1 || (vMaj == 1 && vMin > 3))
	{
		printf("Error: CR2 generated by later version of RIT Darkroom, upgrade me\n");
		return 0;
	}
    else if (vMaj == 1 && vMin < 3)
    {
        printf("Error: CR2 generated by older version of RIT Darkroom, re-import\n");
        return 0;
    }
	
#ifdef WIN32
	CR2Info cr2, cr2_s;
#else	
	CR2Info cr2("192.168.1.81", 22470), cr2_s("192.168.1.81", 22470);
#endif
	Image* img = NULL;
	
	if (cropRect.w <= sensorRect.w && cropRect.h <= sensorRect.h)
	{
		img = CR2_Develop(cr2, fsrc);
		
        if (!img)
            return 0;

		if (cropRect.w < sensorRect.w || cropRect.h < sensorRect.h)
		{
			Image* cimg = img->crop(cropRect.x + 4, cropRect.y + 4, cropRect.w - 8, cropRect.h - 8);
			delete img;
			img = cimg;
		}
	}	
	else
	{	
		img = CR2_DevelopSplit(&cr2, &cr2_s, fsrc, cropRect.w, cropRect.h);
		
        if (!img)
            return 0;

		Image* cimg = img->crop(4, 4 + cropRect.y, cropRect.w - 8, cropRect.h - 8);		
		delete img;
		img = cimg;
	}
	
	
	if (img != NULL)
	{
		img = CR2_PostProcess(info, img, fsrc);
	}
	
	
	//Image* img = new Image(sensorRect.w, sensorRect.h, eImageFormat_RGB);
	
	if (img != NULL)
	{
		String fname(fdst ? fdst : fsrc);
		
		if (!fdst)
		{
			fname = fname.substr(0, fname.length() - 3) + (info->tiff ? "TIF" : "JPG");
		}
		
		ExifIfd& exif = rawsrc.getExif();
		
		if (exif.find(0xA434) == (size_t) -1)
		{
			exif.addTag(0xA434)->setString(lensName);
		}
		
		if (camBrand.length() > 0)
		{
			size_t id = exif.find(0x10F);
			if (id != (size_t) -1)
				exif.at(id)->setString(camBrand);
		}
		
		saveImage(fname.c_str(), *img, info->tiff, &exif);
		
		delete img;
	}

	return 1;
}
