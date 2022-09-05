#include "stdafx.h"
#include "CR2Info.h"
#include "savejpeg.h"
#include "SaveTiff.h"
#include "CR2Server.h"
#include <Windows.h>
#include "denoise.h"
#include "sharpen.h"
#include "image.h"
#include "resample.h"
#include "tile.h"

void ShowHelp();
int process(CR2Info& cr2, string fcr2, string fout, float ec);
bool fileExists(string &f);

const int THUMB_W = 1275, THUMB_H = 850;

int main22(void)
{
    uint32_t w, h, bpp = 24;
    uint8_t* data = LoadJPEG(w, h, bpp, "d:\\test.jpg");
    Image img(w, h, eImageFormat_RGB);

    img.fromRGB8((uint8_t*) data);
    Denoise_Wavelet(&img, 0.28f, 0.2f, 2.8f, 0.08f, 0.3f);
    
    img.toFormat(eImageFormat_RGB);
    img.toRGB8((uint8_t*) data);

    SaveJPEG(data, w, h, bpp, "d:\\test2.jpg", 97);

    return 0;
}

Image* CR2_DevelopSplit(const char* file, int dw, int dh)
{
    string path(file);

    CR2Info cr2;
    cr2.open(path);

    const int sw = cr2.width(), sh = cr2.height();
    uint16_t* raw = new uint16_t[sw * sh * 3];
    printf("Developing %s\n", path.c_str());
    cr2.setAdjustment("Exposure", "0.7");
    cr2.getFull((void*) raw);
    cr2.close();
    Image* img1 = new Image(sw, sh, eImageFormat_RGB);
    img1->fromRGB16(raw);

    path[path.length() - 1] = '_';
    printf("Developing %s\n", path.c_str());
    cr2.open(path);
    cr2.setAdjustment("Exposure", "0.7");
    cr2.getFull((void*) raw);
    cr2.close();
    Image* img2 = new Image(sw, sh, eImageFormat_RGB);
    img2->fromRGB16(raw);
    delete[] raw;

    rect_t rgn[8], fit[8];
    int ts = Tile_Combine(dw, dh, sw - 8, sh - 8, rgn, fit, 8);

    Image* ret = new Image(dw, dh, eImageFormat_RGB);

    printf("Combine results");
    for (int c = 0; c < 3; c ++)
        img1->blit(ret, c, c, fit[0].x, fit[0].y + 3, rgn[0].w, rgn[0].h, rgn[0].x, rgn[0].y);

    for (int s = 0; s < ts; s ++)
    {
        for (int c = 0; c < 3; c ++)
        {
            Image* src = (s == 0) ? img1 : img2;
            src->blit(ret, c, c, fit[s].x, fit[s].y + 3, rgn[s].w, rgn[s].h, rgn[s].x, rgn[s].y);
        }
    }

    delete img1;
    delete img2;

    return ret;
}

int main(void)
{
    const char* file = "D:\\Private\\cr2raw\\Release\\IMG_8002_RIT.CR2";
    const char* ofile = "d:\\split_7d.jpg";

    int dw = 5184, dh = 3456;

    // TODO: Get dimensions from EXIF UserData
    Image* img = CR2_DevelopSplit(file, dw + 8, dh + 8);
    Image* cimg = img->crop(4, 4, dw, dh);
    delete img;

    uint8_t* buf = (uint8_t*) malloc(cimg->width() * cimg->height() * 3);
    cimg->toRGB8(buf);
	//printf("Saving output 16-bit TIFF..\n");
	//SaveTIFF(buf, cimg->width(), cimg->height(), 48, ofile);
    printf("Saving output JPG...\n");
    SaveJPEG(buf, cimg->width(), cimg->height(), 24, ofile, 75);
    free(buf);

    delete cimg;

    return 0;
}

int main5(void)
{
    const char* file = "D:\\Private\\cr2raw\\Release\\2U4A2715_RIT.CR2";
    const char* ofile = "d:\\split_crop.tif";

    CR2Info cr2;
    cr2.open(file);
    const int sw = cr2.width(), sh = cr2.height();
    uint16_t* raw = new uint16_t[sw * sh * 3];
    printf("Developing %s\n", file);
    cr2.getFull((void*) raw);
    cr2.close();

    SaveTIFF(raw, cr2.width(), cr2.height(), 48, ofile);

    delete[] raw;

    return 0;
}

int main3(int argc, char* argv[])
{
    printf("CR2Info by FunkyRider @ LDSClub.net. Build v10\n");
    if (argc < 3)
    {
        ShowHelp();
        return 0;
    }

    void *buf;
    CR2Info cr2;
    cr2.open(argv[2]);
    int ret;

    switch (argv[1][1])
    {
    case 'e':
        printf("%s", cr2.getExif().c_str());        
        break;
    case 'a':
        printf("%s", cr2.getAdjustments().c_str());
        break;
    case 'c':
        {
            string fnin(argv[2]);
            string fnout(argc > 3 ? argv[3] : "");
            if (fnout.length() == 0)
            {
                size_t dot = fnin.find_last_of('.');
                {
                    fnout = fnin.substr(0, dot) + ".JPG";
                }
            }

            if (!fileExists(fnin))
            {
                printf("Error: File does not exist: %s\n", fnin.c_str());
                return -1;
            }
            process(cr2, fnin, fnout, 0);
        }

        break;
    case 't':
        {
            string fnin(argv[2]);
            string fnout(argc > 3 ? argv[3] : "");
            if (fnout.length() == 0)
            {
                fnout = fnin.substr(0, fnin.find_last_of('.')) + "_thumb.JPG";
            }

            if (!fileExists(fnin))
            {
                printf("Error: File does not exist: %s\n", fnin.c_str());
                return -1;
            }
            buf = malloc(THUMB_W * THUMB_H * 3);
            cr2.getScaled(buf, THUMB_W, THUMB_H);
            SaveJPEG(buf, THUMB_W, THUMB_H, 24, fnout.c_str(), 80);
            free(buf);
        }
        break;
    case 's':
        ret = cr2.setAdjustment(argv[3], argv[4]);
        if (ret == 0)
        {
            cr2.save();
            printf("%s", cr2.getAdjustments().c_str());
        }
        else
        {
            printf("Error: Unknown adjustment: %s\n", argv[3]);
        }
        break;

    case 'v':
        // READ VRD
        cr2.close();
        {
            unsigned int pos, len;
            CR2Info::ReadVRD(argv[2], pos, len);

            if (len > 0)
            {
                printf("VRD data block: 0x%08X:0x%08X\n", pos, len);
            }
            else
            {
                printf("Error: Invalid VRD data in file\n");
            }
        }
        break;
    case 'd':
        {
            // Daemon mode
            int port = atoi(argv[2]);

            printf("Starting CR2Server daemon at port %d...\n", port);
            CR2Server server(port);

            server.wait();
        }
        break;
    default:
        ShowHelp();
        break;
    }

    return 0;
}

void ShowHelp()
{
    printf("Usage: cr2info -? [file.cr2 | port] [file.tif | param value]\n\t-e: Get EXIF\n\t-a: Get adjustment\n\t-s: Set adjustment\n\t-c: Convert to JPG/TIFF\n\t-t: Thumbnail (1275 x 850)\n\t-v: Read VRD block info\n\t-d: Starts in daemon mode with given port\n");
}

bool fileExists(string &f)
{
    if (FILE *file = fopen(f.c_str(), "r")) {
        fclose(file);
        return true;
    } else {
        return false;
    }  
}

string getExifValue(const string &data, string name)
{
	name += ": ";
	size_t l = name.length();
	size_t p = data.find(name);
	size_t p2 = data.find("\n", p);
	
	return data.substr(p + l, p2 - p - l);
}

float WAVELET_AMT[6][6] = {
    {0.25f, 0.7f, 0.8f, 0.2f, 4096, 0.0f},
    {0.3f, 0.6f, 1.0f, 0.15f, 2048, 0.1f},
    {0.3f, 0.5f, 1.6f, 0.12f, 1024, 0.2f},
    {0.55f, 0.4f, 2.3f, 0.08f, 512, 0.3f},
    {0.85f, 0.35f, 3.8f, 0.04f, 512, 0.5f},
    {1.05f, 0.35f, 6.2f, 0.03f, 512, 0.7f},
};

int32_t num_hash_values(uint16_t *ptr, int32_t size, int32_t strip)
{
    int32_t *hash = new int32_t[65536];

    memset(hash, 0, sizeof (int32_t) * 65536);

    for (int32_t i = 0; i < size; i ++)
    {
        const uint16_t s = ptr[i * strip];
        hash[s] ++;
    }

    int32_t c = 0;
    for (int32_t i = 0; i < 65536; i ++)
    {
        if (hash[i] > 0) c ++;
    }

    return c;
}

int process(CR2Info& cr2, string fcr2, string fout, float ec)
{
	//cr2.open(fcr2.c_str());
	string exif = cr2.getExif();
	string adjustments = cr2.getAdjustments();
	
	printf("%s\n", exif.c_str());
	
	int iso = atoi(getExifValue(exif, "ISO").c_str());	
	float aperture = atof(getExifValue(exif, "Aperture").substr(1).c_str());
	string szfocal = getExifValue(exif, "FocalLength");
	string szModel = getExifValue(exif, "Model");
	size_t p = szfocal.find("mm");
	int focal = atoi(szfocal.substr(0, p).c_str());
	
	const uint32_t w = cr2.width(), h = cr2.height();
	int nrlevel = 0;
	int sharpness = 0;//4;
	
	float fec = atof(getExifValue(adjustments, "Exposure").c_str());
	
	ec += fec;
	
	if (ec < -2)
		ec = -2;
	else if (ec > 2)
		ec = 2;
		
	if (szModel == "EOS 5D")
	{
		// 5D Crashes on custom picture styles
		cr2.setAdjustment("PictureStyle", "132");
	}
	
            //cr2.setAdjustment("Contrast", "0");
            //cr2.setAdjustment("Saturation", "1");
            //cr2.setAdjustment("Exposure", "1.0");
            //cr2.setAdjustment("Sharpness", "3");
            //cr2.setAdjustment("WBShift", "0,-2");


	if (ec != 0)
	{
		char buf[8];
		sprintf(buf, "%.1f", ec);
		
		cr2.setAdjustment("Exposure", buf);
		iso = (int)((float) iso * pow(2, ec));
		printf("Effective ISO: %d\n", iso);
	}
	
	if (iso > 8000)
	{
		nrlevel = 6;
		sharpness = 0;		
	}
	else if (iso > 2000)
	{
		nrlevel = (iso > 3200) ? 5 : 4;
		sharpness = 2;
		if (iso > 3200)
		{
			if (iso > 6400)
				sharpness = 1;
		}
	}
	else if (iso >= 1000)
	{
		nrlevel = 3;
		sharpness = 2;
	}
	else if (iso >= 640)
	{
		nrlevel = 2;
		sharpness = 3;
	}
	else if (iso >= 320)
	{
		nrlevel = 1;
		sharpness = 4;
	}
	else if (iso >= 200)
	{
		nrlevel = 0;
		sharpness = 4;
	}

	char szbuf[8];
	sprintf_s(szbuf, 8, "%d", sharpness);
	//cr2.setAdjustment("Sharpness", "7");
	
	string adjs = cr2.getAdjustments();
	printf("%s\n", adjs.c_str());

    Image *img = NULL;

    {
	    uint16_t* buf = (uint16_t*) malloc(w * h * 6);
	    printf("Processing raw data...\n");
	    cr2.getFull(buf);

        /*
        for (int32_t i = 0; i < w * h * 3; i ++)
        {
            buf[i] &= 0xFF00;
        }
        int32_t c = num_hash_values(buf, w * h, 3);
        printf("Number of unique brightness in channel R: %d\n", c);
        c = num_hash_values(buf + 1, w * h, 3);
        printf("Number of unique brightness in channel G: %d\n", c);
        c = num_hash_values(buf + 2, w * h, 3);
        printf("Number of unique brightness in channel B: %d\n", c);
        */

        img = new Image(w, h, eImageFormat_RGB);
        img->fromRGB16(buf);
        free(buf);
    }


	if (focal > 1 && focal <= 22)
	{		
		static float CA_RED[] = {-0.4f, -0.6f, -0.8f, -1.0f, -1.2f, -1.4f, -1.6f};
		
		int ca_id = 22 - focal;
		if (ca_id > 6)
			ca_id = 6;

        printf("Correcting CA (Red: %.2f, Blue: %.2f)...\n", CA_RED[ca_id], 0);
        Resample_Channel(img, 0, CA_RED[ca_id], 0.0f, 0.0f);
    }

    /*
	if (nrlevel > 0)
	{
		printf("Noise reduction (level %d)...\n", nrlevel);

        nrlevel --;
		float imp = sqrtf(WAVELET_AMT[nrlevel][4]) / 255.0f;
        Image* res = Denoise_Impulse(img, imp * imp);
        if (img != res) { delete img; img = res; }

        res = Denoise_Wavelet(img, WAVELET_AMT[nrlevel][0], WAVELET_AMT[nrlevel][1], WAVELET_AMT[nrlevel][2], WAVELET_AMT[nrlevel][3], WAVELET_AMT[nrlevel][5]);
        if (img != res) { delete img; img = res; }
	}
    */
    
	{
		int stype = 0;
		float samt = 0, bamt = 0, th = 0;
		
		if (aperture <= 4)
		{
			if (iso <= 200)
			{
				stype = 1;	samt = 0.14f;	bamt = -0.02f;	th = 24.0f / 255.0f;
			}
			else if (iso <= 1000)
			{
				stype = 1;	samt = 0.24f;	bamt = 0.04f;	th = 24.0f / 255.0f;
			}
			else if (iso <= 2000)
			{
				stype = 1;	samt = 0.30f;	bamt = 0.06f;	th = 32.0f / 255.0f;
			}
			else if (iso <= 4000)
			{
				stype = 1;	samt = 0.34f;	bamt = 0.12f;	th = 32.0f / 255.0f;
			}
			else 
			{
				stype = 1;	samt = 0.38f;	bamt = 0.18f;	th = 48.0f / 255.0f;
			}
		}
		else
		{
			if (iso <= 400)
			{
				if (aperture > 11)
				{
					stype = 0;	samt = 0.16f;
				}
				else
				{
					stype = 0;	samt = 0.11f;
				}
			}
			else
			{
				stype = 1;	samt = 0.1f;	bamt = 0.0f;	th = 32.0f / 255.0f;
			}
		}
	/*
        if (stype == 0)
		{
			printf("Sharpening (%.2f px)...\n", samt);
			Image* res = Sharpen(img, samt);
			if (img != res) { delete img; img = res; }
		}
		else
		{
			printf("Edge Sharpening (%.2f px, %.2f px, %.2f threshold)...\n", samt, bamt, th);
			Image* res = Sharpen2(img, samt, bamt, th);
			if (img != res) { delete img; img = res; }
		}*/
	}

    {
        //printf("Sharpening...\n");
        //Image* res = SharpenRIT(img, 0.48f);
        //if (img != res) { delete img; img = res; }
    }


    int wb = atoi(getExifValue(adjustments, "WhiteBalance").c_str());
    if (szModel == "EOS-1D" && wb != 6 && wb < 10)
    {
        printf("Fixing green tint for EOS-1D...\n");

        img->toFormat(eImageFormat_YCbCr);
        const int32_t size = img->width() * img->height();

        float* cb = img->channel(1);
        for (int32_t i = 1; i < size; i ++)
        {
            cb[i] += 0.012f;
        }

        float* cr = img->channel(2);
        for (int32_t i = 0; i < size; i ++)
        {
            cr[i] += 0.008f;
        }
    }

    {
        img->toFormat(eImageFormat_RGB);

	    if (fout.find(".TIF") != string::npos)
	    {
	        uint16_t* buf = (uint16_t*) malloc(w * h * 6);
            img->toRGB16(buf);
		    printf("Saving output 16-bit TIFF..\n");
		    SaveTIFF(buf, w, h, 48, fout.c_str());
    	    free(buf);
	    }
	    else
	    {
	        uint8_t* buf = (uint8_t*) malloc(w * h * 3);        
            img->toRGB8(buf);
		    printf("Saving output JPG...\n");
		    SaveJPEG(buf, w, h, 24, fout.c_str(), 96);
    	    free(buf);
	    }
    }

    delete img;

	return 0;
}