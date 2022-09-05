#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <memory.h>
#include "cr2plugins.h"

#include "SaveTiff.h"

const uint16_t _BLACK = 127, _SATURATION = 3712;
const uint16_t _BLACK_14 = 1023, _SATURATION_14 = 15280;

int32_t sw, sh;
uint16_t* sdata;
uint16_t* sinfo;

void putpixel(int x, int y, int c)
{
    //x = ((x >> 1) << 1);
    //y = ((y >> 1) << 1);
    sdata[(y + sinfo[6]) * sw + x + sinfo[5]] = c + _BLACK;
}

void putpixel_14(int x, int y, int c)
{
    //x = ((x >> 1) << 1);
    //y = ((y >> 1) << 1);
    sdata[(y + sinfo[6]) * sw + x + sinfo[5]] = c + _BLACK_14;
}

void putrect(int x, int y, int w, int h, int c)
{
    for (int i = 0; i < h; i ++)
    {
        for (int j = 0; j < w; j ++)
        {
            putpixel(j + x, i + y, c);
        }
    }
}

void putrect_14(int x, int y, int w, int h, int c)
{
    for (int i = 0; i < h; i ++)
    {
        for (int j = 0; j < w; j ++)
        {
            putpixel_14(j + x, i + y, c);
        }
    }
}


// Gray tone curve
void Pattern1()
{
    char path[512];
	strcpy(path, getAppPath());
	strcat(path, "1DS2_BLK.CR2");
    CR2Raw* src = new CR2Raw(path);
    sw = src->getWidth(), sh = src->getHeight();
    sdata = src->createRawData(sw, sh - 100, _BLACK);
    sinfo = src->getSensorInfo();

    putrect(100, 100, 100, 100, 1024);

    /*for (int i = 0; i < 53; i ++)
    {
        for (int j = 0; j < 20; j ++)
        {
            putrect(j * 60, i * 60, 56, 56, i * 70 + j);
        }
    }*/

    uint16_t* slices = src->getSliceData();

    slices[0] = 2;
    slices[1] = 1920;
    slices[2] = sw - 3840;

    sinfo[2] -= 100;
    sinfo[8] -= 100;

	strcpy(path, getAppPath());
	strcat(path, "Pattern1.CR2");
	printf("Saving: %s\n", path);
    src->save(path);
    delete src;
}

void Pattern1_14()
{
    char path[512];
	strcpy(path, getAppPath());
	strcat(path, "1DS3_BLK.CR2");
    CR2Raw* src = new CR2Raw(path);
    sw = src->getWidth(), sh = src->getHeight();
    sdata = src->createRawData(sw, sh, _BLACK_14);
    sinfo = src->getSensorInfo();

    for (int i = 0; i < 53; i ++)
    {
        for (int j = 0; j < 70; j ++)
        {
            putrect_14(j * 60, i * 60, 56, 56, i * 280 + j * 4);
        }
    }

	strcpy(path, getAppPath());
	strcat(path, "Pattern1_14.CR2");
	printf("Saving: %s\n", path);
    src->save(path);
    delete src;
}

// Sharpness
void Pattern2()
{
    char path[512];
	strcpy(path, getAppPath());
	strcat(path, "1DS3_BLK.CR2");
    CR2Raw* src = new CR2Raw(path);
    sw = src->getWidth(), sh = src->getHeight();
    sdata = src->createRawData(sw, sh, _BLACK);
    sinfo = src->getSensorInfo();

    for (int i = 0; i <= 10; i ++)
    {
        putrect(128, 64 + i * 290, 4750, 256, i * 1425);//350);

        for (int j = 0; j <= 100; j ++)
        {
            putrect(256 + j * 45, 64 + i * 290 + 64, 38, 128, j * 142);//35);
        }
    }

	strcpy(path, getAppPath());
	strcat(path, "Pattern2.CR2");
	printf("Saving: %s\n", path);
    src->save(path);
    delete src;
}

// Red tone curve
void Pattern3()
{
    char path[512];
	strcpy(path, getAppPath());
	strcat(path, "1DS2_BLK.CR2");
    CR2Raw* src = new CR2Raw(path);
    sw = src->getWidth(), sh = src->getHeight();
    sdata = src->createRawData(sw, sh, _BLACK);
    sinfo = src->getSensorInfo();

    for (int i = 0; i < 53; i ++)
    {
        for (int j = 0; j < 70; j ++)
        {
            putrect(j * 60, i * 60, 56, 56, i * 70 + j);
        }
    }

	strcpy(path, getAppPath());
	strcat(path, "Pattern3.CR2");
	printf("Saving: %s\n", path);
    src->save(path);
    delete src;
}

void Resize()
{
    char path[512];
	strcpy(path, getAppPath());
	strcat(path, "1DS2_BLK.CR2");
    CR2Raw* src = new CR2Raw(path);
    sw = src->getWidth(), sh = src->getHeight();
    sdata = src->createRawData(sw, sh, _BLACK);
    sinfo = src->getSensorInfo();

    sinfo[7] -= 30;
    sinfo[8] -= 20;

    putrect(512, 512, 512, 512, 512);

	strcpy(path, getAppPath());
	strcat(path, "Resize.CR2");
	printf("Saving: %s\n", path);
    src->save(path);
    delete src;
}


int main(int argc, char** argv)
{
    Pattern1();
    //Pattern1_14();

    return 0;
}