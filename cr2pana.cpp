#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <memory.h>
#include "cr2plugins.h"

#include "SaveTiff.h"

static const LensInfo _lens[] = {
    // LUMIX
	{"LUMIX G 14/F2.5", "", 0, {
        {14, 1.0006360f, 1.0002741f}}
    },
    {"LUMIX G 20/F1.7", "", 0, {
        {20, 1.0003463f, 1.0001875f}}
    },
    {"LUMIX G 20/F1.7 II", "", 0, {
        {20, 1.0003463f, 1.0001875f}}
    },
    {"LUMIX G VARIO 7-14/F4.0", "", 0, {
        {7, 1.0002675f, 1.0001169f},
        {8, 1.0002604f, 1.0002017f},
        {10, 1.0000787f, 1.0001936f},
        {12, 1.0001091f, 1.0001823f},
        {14, 1.0002823f, 1.0001142f}}
    },
    {"LUMIX G VARIO 12-35/F2.8", "", 0, {
        {12, 1.0003736f, 1.0000643f},
        {18, 1.0003571f, 1.0000159f},
        {25, 1.0002534f, 1.0000103f},
        {30, 1.0001151f, 1.0000065f},
        {35, 1.0f, 1.0003f}}
    },
    {"LUMIX G VARIO PZ 14-42/F3.5-5.6", "", 0, {
        {14, 0.9999729f, 1.0005586f},
        {18, 1.0000205f, 1.0005124f},
        {25, 1.0000210f, 1.0003060f},
        {35, 0.9999745f, 1.0001376f},
        {42, 1.0000187f, 1.0000910f}}
    },
    {"LUMIX G VARIO 14-140/F4.0-5.8", "", 0, {
        {14, 1.0002346f, 1.0001381f},
        {18, 1.0001169f, 1.0001872f},
        {25, 1.0000834f, 1.0001885f},
        {32, 0.9999374f, 1.0001907f},
        {48, 0.9998177f, 1.0001548f},
        {70, 0.9994949f, 1.0003511f},
        {99, 0.9991839f, 1.0003344f},
        {140, 0.9993829f, 0.9999327f}}
    },
    {"LUMIX G VARIO PZ 45-175/F4.0-5.6", "", 0, {
        {45, 0.9999469f, 1.0003685f},
        {60, 0.9998969f, 1.0004256f},
        {75, 0.9997525f, 1.0004488f},
        {100, 0.9996669f, 1.0003098f},
        {150, 0.9996701f, 0.9999976f},
        {175, 0.9996807f, 0.9999970f}}
    },
    {"LUMIX G VARIO 45-200/F4.0-5.6", "", 0, {
        {45, 1.0003614f, 1.0001921f},
        {58, 1.0002287f, 1.0000954f},
        {75, 1.0002702f, 0.9999980f},
        {103, 0.9998652f, 0.9995406f},
        {158, 0.9988054f, 0.9992661f},
        {200, 0.9981137f, 0.9992274f}}
    },
    {"LUMIX G VARIO 100-300/F4.0-5.6", "", 0, {
        {100, 0.9999865f, 1.0001016f},
        {150, 0.9999362f, 0.9998824f},
        {201, 0.9997969f, 0.9997401f},
        {252, 0.9997044f, 0.9993205f},
        {300, 0.9995055f, 0.9989849f}}
    },

    // LEICA
    {"LEICA DG SUMMILUX 25/F1.4", "", 0, {
        {25, 1.0002675f, 0.9998339f}}
    },
    {"LEICA DG MACRO-ELMARIT 45/F2.8", "", 0, {
        {45, 1.0005f, 0.9998f}}
    },

    // OLYMPUS
    {"OLYMPUS M.17mm F1.8", "", 0, {
        {17, 1.0002674f, 0.9997845f}}
    },
    {"OLYMPUS M.17mm F2.8", "", 0, {
        {17, 1.0016410f, 0.9989794f}}
    },
    {"OLYMPUS M.45mm F1.8", "", 0, {
        {45, 1.0001904f, 0.9999203f}}
    },
    {"OLYMPUS M.75mm F1.8", "", 0, {
        {75, 0.9999905f, 0.9999613f}}
    },
    {"OLYMPUS M.9-18mm F4.0-5.6", "", 0, {
        {9, 1.0001617f, 1.0002941f},
        {10, 1.0000787f, 1.0003066f},
        {13, 0.9999052f, 1.0004586f},
        {14, 0.9999490f, 1.0004648f},
        {16, 1.0000189f, 1.0004628f},
        {18, 1.0000589f, 1.0003623f}}
    },
    {"OLYMPUS M.12-40mm F2.0", "", 0, {
        {12, 1.0004733f, 0.9998876f},
        {19, 1.0003633f, 0.9998815f},
        {26, 1.0002821f, 0.9998613f},
        {32, 1.0001670f, 0.9999078f},
        {40, 0.9999588f, 0.9999736f}}
    },
    {"OLYMPUS M.14-42mm F3.5-5.6", "", 0, {
        {14, 1.0002908f, 1.0f},
        {18, 1.0004544f, 1.0007081f},
        {25, 1.0005611f, 1.0004058f},
        {30, 1.0006295f, 1.0002145f},
        {35, 1.0006484f, 1.0000846f},
        {42, 1.0006187f, 1.9999271f}}
    },
    {"OLYMPUS M.14-42mm F3.5-5.6 L", "", 0, {
        {14, 1.0001242f, 1.0007041f},
        {18, 1.0000281f, 1.0007117f},
        {25, 1.0001315f, 1.0002999f},
        {35, 1.0002742f, 1.0002395f},
        {42, 1.0004259f, 1.0000256f}}
    },
   {"OLYMPUS M.14-42mm F3.5-5.6 II", "", 0, {
        {14, 1.0001828f, 1.0004280f},
        {18, 1.0000985f, 1.0002535f},
        {25, 1.0001388f, 1.0005537f},
        {30, 1.0002040f, 1.0002187f},
        {35, 1.0003653f, 1.0003605f},
        {42, 1.0003152f, 1.0001196f}}
    },
    {"OLYMPUS M.14-42mm F3.5-5.6 II R", "", 0, {
        {14, 1.0001828f, 1.0004280f},
        {18, 1.0000985f, 1.0002535f},
        {25, 1.0001388f, 1.0005537f},
        {30, 1.0002040f, 1.0002187f},
        {35, 1.0003653f, 1.0003605f},
        {42, 1.0003152f, 1.0001196f}}
    },
    {"OLYMPUS M.14-150mm F4.0-5.6", "", 0, {
        {14, 1.0005298f, 0.9998478f},
        {25, 1.0002537f, 1.0000552f},
        {45, 0.9998261f, 1.0002042f},
        {70, 0.9992661f, 1.0005723f},
        {100, 0.9991092f, 1.0003522f},
        {150, 0.9991318f, 1.0001241f}}
    },
};

static const CamInfo _template_cam[] = {
    {"1DS2_BLK.CR2",			1, 127, 0xe80, {6517,-602,-867,-8180,15926,2378,-1618,1771,7633}, {7565,-1791,-273,-6727,14066,2934,-820,1093,7688}},
    {"1DS3_BLK.CR2",			1, 1023, 0x3bb0, {0}, {0}},
};

static const CamInfo _cam[] = {
    {"DMC-G1", 0, 0, 4095, {8199,-2065,-1056,-8124,16156,2033,-2458,3022,7220}, {8929,-2933,-601,-6823,14629,2363,-1271,2024,7792}, {1.0f, 1.0f, 1.0f}},
    {"DMC-G2", 0, 0, 4095, {10113,-3400,-1114,-4765,12683,2317,-377,1437,6710}, {10433,-4550,-301,-3842,11107,3148,-349,1097,7668}, {1.0f, 1.0f, 1.0f}},
    {"DMC-G3", 0, 0, 4095, {6763,-1919,-863,-3868,11515,2684,-1216,2387,5879}, {6677,-2053,-634,-3371,10566,3253,-725,1551,6614}, {1.0f, 1.0f, 1.0f}},
    {"DMC-G5", 0, 0, 4095, {7798,-2562,-740,-3879,11584,2613,-1055,2248,5434}, {8087,-3110,-172,-3294,10463,3289,-401,1311,6290}, {1.0f, 1.0f, 1.0f}},
    {"DMC-G6", 0, 0, 4095, {8294,-2891,-651,-3869,11590,2595,-1183,2267,5352}, {8900,-3755,270,-3407,10753,3068,-599,1555,6269}, {1.0f, 1.0f, 1.0f}},
    {"DMC-G10", 0, 0, 4095, {10113,-3400,-1114,-4765,12683,2317,-377,1437,6710}, {10433,-4550,-301,-3842,11107,3148,-349,1097,7668}, {1.0f, 1.0f, 1.0f}},
    {"DMC-GF1", 0, 0, 4095, {7888,-1902,-1011,-8106,16085,2099,-2353,2866,7330}, {8329,-2548,-472,-6882,14563,2514,-1226,1911,7837}, {1.0f, 1.0f, 1.0f}},
    {"DMC-GF2", 0, 0, 4095, {7888,-1902,-1011,-8106,16085,2099,-2353,2866,7330}, {8329,-2548,-472,-6882,14563,2514,-1226,1911,7837}, {1.0f, 1.0f, 1.0f}},
    {"DMC-GF3", 0, 0, 4095, {9051,-2468,-1204,-5212,13276,2121,-1197,2510,6890}, {8881,-2735,-864,-4505,12138,2675,-602,1511,7893}, {1.0f, 1.0f, 1.0f}},
    {"DMC-GF5", 0, 0, 4095, {8228,-2945,-660,-3938,11792,2430,-1094,2278,5793}, {8404,-3597,312,-3368,10478,3357,-498,1353,6882}, {1.0f, 1.0f, 1.0f}},
    {"DMC-GF6", 0, 0, 4095, {8130,-2801,-946,-3520,11289,2552,-1314,2511,5791}, {8514,-3399,-455,-3000,10247,3207,-665,1570,6769}, {1.0f, 1.0f, 1.0f}},
    {"DMC-GH1", 0, 0, 4095, {6299,-1466,-532,-6535,13852,2969,-2331,3112,5984}, {6928,-2146,-114,-5228,12283,3343,-1230,2111,6223}, {1.0f, 1.0f, 1.0f}},
    {"DMC-GH2", 0, 0, 4095, {7780,-2410,-806,-3913,11724,2484,-1018,2390,5298}, {7970,-2864,-409,-3281,10620,3083,-500,1504,6050}, {1.0f, 1.0f, 1.0f}},
    {"DMC-GH3", 0, 0, 4095, {6559,-1752,-491,-3672,11407,2586,-962,1875,5130}, {6721,-2283,393,-2946,9993,3452,-312,974,5864}, {1.0f, 1.0f, 1.0f}},
    {"DMC-GH4", 0, 0, 4095, {7122,-2108,-512,-3155,11201,2231,-541,1423,5045}, {7726,-2915,118,-4213,12198,2259,-476,1114,6346}, {1.0f, 1.0f, 1.0f}},
    {"DMC-GM1", 0, 0, 4095, {6770,-1895,-744,-5232,13145,2303,-1664,2691,5703}, {9784,-4995,30,-3625,11454,2475,-961,2097,6377}, {1.0f, 1.0f, 1.0f}},
    {"DMC-GX1", 0, 0, 4095, {6763,-1919,-863,-3868,11515,2684,-1216,2387,5879}, {6677,-2053,-634,-3371,10566,3253,-725,1551,6614}, {1.0f, 1.0f, 1.0f}},
    {"DMC-GX7", 0, 0, 4095, {7610,-2780,-576,-4614,12195,2733,-1375,2393,6490}, {8239,-3692,251,-3913,11077,3267,-631,1486,7468}, {1.0f, 1.0f, 1.0f}},
};

int main(int argc, char** argv)
{
	int template_id = 0;

    printf("CR2Pana v0.4 Alpha - 20141006\n");

    if (argc < 2)
	{
        printf("    Panasonic RW2 to Canon CR2 (12-bit) conversion\n"
                "    This software is provided AS IS without warranty of any kind\n"
			"    Author: B.Zhang, ndawinteractive@gmail.com\n"
			"    Special thanks to: Kent, Dave Coffin, LensFun Project"
            "\n"
			"Usage: cr2pana Input.RW2 [Output.CR2]\n"
            "\n"
            "Supported cameras:\n");

        for (uint32_t i = 0; i < COUNT(_cam); i ++)
		{
		    printf("    Panasonic %s\n", _cam[i].model);
		}

        printf("\nCorrected lenses:\n");

		for (uint32_t i = 0; i < COUNT(_lens); i ++)
		{
			printf("    %s\n", (_lens[i].dispName[0] != 0) ? _lens[i].dispName : _lens[i].model);
		}

        return 0;
    }

    printf("Loading: %s\n", argv[1]);
    CR2Raw* dest = new CR2Raw(argv[1]);	
	const int32_t dw = dest->getWidth();//, dh = dest->getHeight();
    uint16_t* ddata = dest->getRawData();
    const uint32_t* blksatwb = dest->getRW2BlackSatWB();
    
	char path[512];
	strcpy(path, getAppPath());
	strcat(path, _template_cam[template_id].model);
    CR2Raw* src = new CR2Raw(path);
    const int32_t sw = src->getWidth(), sh = src->getHeight();
    uint16_t* sdata = src->createRawData(sw, sh, blksatwb[0]);

    const char* name = dest->getModel();
    int32_t id = -1;

    printf("Camera: %s\n", name);
    printf("Black: %d, Saturation: %d\n", blksatwb[0], blksatwb[3]);
    //printf("WB Coeff: %d, %d, %d\n", blksatwb[6], blksatwb[7], blksatwb[8]);

    for (uint32_t i = 0; i < COUNT(_cam); i ++)
    {
        if (strcmp(name, _cam[i].model) == 0)
        {
            id = i;
            break;
        }
    }

    if (id < 0)
    {
        printf("Unsupported model, exiting.\n");
        return -1;
    }
    
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
        	xstart ++;
        	if (diff_w > 0) xwidth --;
        	break;
        case 0x16161616:	// RG/GB
        	ystart ++;
        	if (diff_h > 0) ywidth --;
        	break;
        case 0x49494949:	// GR/BG, 1Ds2 native   // good
        default:
        	break;
        }

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
	bayer->fromRAW16(sdata + sw, _cam[id].saturation, blksatwb[0]);

	// CA correction
	bayer = CR2_profiled_lens(bayer, src, _lens, COUNT(_lens));
	
	// Denoise
	bayer = CR2_profiled_denoise(bayer, dest->getISO());
    
    // Color space transform
    {
        float mat_d65[9];
        FORC(9) mat_d65[c] = (float) _cam[id].cam_xyz_d65[c] / 10000.0f;
        const float r = sqrtf(mat_d65[0] * mat_d65[0] + mat_d65[1] * mat_d65[1] + mat_d65[2] * mat_d65[2]);
        const float g = sqrtf(mat_d65[3] * mat_d65[3] + mat_d65[4] * mat_d65[4] + mat_d65[5] * mat_d65[5]);
        const float b = sqrtf(mat_d65[6] * mat_d65[6] + mat_d65[7] * mat_d65[7] + mat_d65[8] * mat_d65[8]);
        const float sr = (float) blksatwb[6] * (r / g), sb = (float) blksatwb[8] * (b / g);
        //printf("sr = %.4f, sb = %.4f, ratio: %.4f\n", sr, sb, sr / sb);

	    float dlmix = (sr / sb - 0.485f) / 1.3f;
        if (dlmix < 0) dlmix = 0; else if (dlmix > 1) dlmix = 1;
        bayer = CR2_profiled_color(bayer, &_cam[id], &_template_cam[template_id], dlmix);
    }

    // WB as shot translation
    {
        const float from[3] = {(float) blksatwb[6], (float) blksatwb[7], (float) blksatwb[8]};
        float to[3];

        CR2_white_point(&_cam[id], &_template_cam[template_id], from, to);

        uint32_t* wb = src->getWBCoefficients();
        wb[0] = (uint16_t) (1024.0f * (to[0] / to[1]));
        wb[1] = 1024;
        wb[2] = 1024;
        wb[3] = (uint16_t) (1024.0f * (to[2] / to[1]));
    }

    // To uint16
    bayer->toRAW16(sdata + sw, _template_cam[template_id].saturation, _template_cam[template_id].black);
    delete bayer;

	// Save output
    {
        char dfile[512];	
		strcpy(dfile, argv[1]);
		size_t l = strlen(dfile);
		dfile[l - 4] = '\0';
		strcat(dfile, ".CR2");

        if (argc >= 3)
		{
			strcpy(dfile, argv[2]);
		}
	
		printf("Saving: %s\n", dfile);

        src->save(dfile, dest);
    }

    delete dest;
    delete src;

    return 0;
}

/*
int main()
{
    //CR2Raw blk("D:\\Private\\cr2raw\\Debug\\1DS2_BLK - Copy.CR2");

    //blk.reformat("D:\\Private\\cr2raw\\Debug\\1DS2_BLK2.CR2");

    FILE* fp = fopen("D:\\Private\\cr2raw\\Release\\1DS2_BLK.CR2", "r+");

    fseek(fp, 0x27B4, SEEK_SET);
    fputc(0x1C, fp);
    fputc(0x28, fp);

    fseek(fp, 0x286E, SEEK_SET);
    fputc(0x76, fp);
    fputc(0x28, fp);
    fputc(0x00, fp);
    fputc(0x00, fp);

    fclose(fp);

    return 0;
}
*/

/*
int main2()
{
    float mat_to_d65[9], mat_to_stda[9];

    for (uint32_t i = 0; i < COUNT(_cam); i ++)
	{
        for (int32_t j = 0; j < 9; j ++)
        {
            mat_to_d65[j] = (float) _cam[i].cam_xyz_d65[j] / 10000.0f;
            mat_to_stda[j] = (float) _cam[i].cam_xyz_stda[j] / 10000.0f;
        }

		printf("    Panasonic %s\n", _cam[i].model);

        float r = sqrtf(mat_to_d65[0] * mat_to_d65[0] + mat_to_d65[1] * mat_to_d65[1] + mat_to_d65[2] *  mat_to_d65[2]);
        float g = sqrtf(mat_to_d65[3] * mat_to_d65[3] + mat_to_d65[4] * mat_to_d65[4] + mat_to_d65[5] *  mat_to_d65[5]);
        float b = sqrtf(mat_to_d65[6] * mat_to_d65[6] + mat_to_d65[7] * mat_to_d65[7] + mat_to_d65[8] *  mat_to_d65[8]);
        float sr = 1 / (r / g), sb = 1 / (b / g);
        printf("    D65 WB.: %.4f, %.4f, %.4f, r/b: %.4f\n", sr, 1.0f, sb, sr/sb);
        
        r = sqrtf(mat_to_stda[0] * mat_to_stda[0] + mat_to_stda[1] * mat_to_stda[1] + mat_to_stda[2] *  mat_to_stda[2]);
        g = sqrtf(mat_to_stda[3] * mat_to_stda[3] + mat_to_stda[4] * mat_to_stda[4] + mat_to_stda[5] *  mat_to_stda[5]);
        b = sqrtf(mat_to_stda[6] * mat_to_stda[6] + mat_to_stda[7] * mat_to_stda[7] + mat_to_stda[8] *  mat_to_stda[8]);
        sr = 1 / (r / g), sb = 1 / (b / g);
        printf("    D28 WB.: %.4f, %.4f, %.4f, r/b: %.4f\n", sr, 1.0f, sb, sr/sb);

	}

    return 0;
}
*/