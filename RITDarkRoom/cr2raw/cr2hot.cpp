#include "os.h"
#include <math.h>
#include <memory.h>

#include "cr2raw.h"
#include "image.h"

int32_t blockmean(uint16_t* data, int32_t w, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
	int32_t yacc = 0;
	
	for (int32_t i = y1; i < y2; i ++)
 	{
 		int32_t xacc = 0;
 		uint16_t* lp = data + i * w;
 		
 		for (int32_t j = x1; j < x2; j ++)
 		{
 			xacc += lp[j];
 		}
 		
 		yacc += xacc / (x2 - x1);
 	}
 	
 	return yacc / (y2 - y1);
}

int CR2_Hot(const char* fsrc)
{
	printf("Loading: %s\n", fsrc);
	CR2Raw* src = new CR2Raw(fsrc);
	const int32_t iso = src->getISO();
	const int32_t w = src->getWidth();
    uint16_t* data = src->getRawData();    
    uint16_t* sinfo = src->getSensorInfo();
    
    const int32_t x1 = sinfo[5], y1 = sinfo[6], x2 = sinfo[7], y2 = sinfo[8];
    const int32_t vmax = (1 << src->getBps()) - 1;
    const int32_t vtol = (vmax / 32) * (vmax / 32);
 	int32_t avgs[5];
 	
 	const int32_t cx = (x2 + x1) / 2, cy = (y2 + y1) / 2;
 	avgs[0] = blockmean(data, w, cx - 128, cy - 128, cx + 128, cy + 128);
 	avgs[1] = blockmean(data, w, x1, y1, x1 + 256, y1 + 256);
 	avgs[2] = blockmean(data, w, x2 - 256, y1, x2, y1 + 256);
 	avgs[3] = blockmean(data, w, x1, y2 - 256, x1 + 256, y2);
 	avgs[4] = blockmean(data, w, x2 - 256, y2 - 256, x2, y2);
 	
 	const int32_t avg = (avgs[0] + avgs[1] + avgs[2] + avgs[3] + avgs[4]) / 5;
 	
 	for (int32_t i = 0; i < 5; i ++)
 	{
 		if (abs(avgs[i] - avg) * 10 > avg)
 		{
 			printf("Unstable black, not a dark frame?\n");
 			return -1;
 		}
	}
	
 	int32_t hot = 0;
 	
 	char fname[512];
 	
 	if (iso >= 400)
 	{
	 	sprintf(fname, "%s_%s.Hot", src->getModel(), src->getSerialNumber());
	}
	else
	{
		sprintf(fname, "%s_%s.ADCBias", src->getModel(), src->getSerialNumber());
	}
	 
 	const uint32_t flen = strlen(fname);
 	for (uint32_t i = 0; i < flen; i ++)
 	{
 		if (fname[i] == ' ') fname[i] = '_';
 	}

 	printf("Output: %s\n", fname);
    char path[512];
	strcpy(path, getAppPath());
	strcat(path, fname);
	
 	FILE* fp = _fopen(path, "wt");
 	if (!fp)
 	{
 		printf("Unable to create file: %s\n", fname);
 		return -1;
 	}
 	
 	printf("Black level: %d\n", avg);
 	
 	if (iso >= 400)
 	{
 		printf("Generating Hot pixel map...\n");
	 	printf("Tolerance: %d\n", (int) sqrtf((float) vtol));
	 	for (int32_t i = y1; i < y2; i ++)
	 	{
	 		uint16_t* lp = data + i * w;
	 		for (int32_t j = x1; j < x2; j ++)
	 		{
	 			const int32_t val = lp[j];
	 			
	 			if ((val - avg) * (val - avg) > vtol)
	 			{
	 				hot ++;
	 				
	 				if (hot < 10)
	 				{
	 					printf("x:%d y:%d [%d]\n", j, i, val);
	 				}
	 				else if (hot == 10)
	 				{
	 					printf("...\n");
	 				}
	 				
	 				fprintf(fp, "%d %d %f\n", j, i, (float) val / (float) avg);
	 			}
	 		}
	 	}

	 	printf("Total hot pixels: %d\n", hot); 	
	}
	else
	{
		printf("Generating ADC colum bias map...\n");
	 	int32_t CDS_COL[8] = {0, 0, 0, 0, 0, 0, 0, 0};
		int32_t CDS_CNT[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	
		for (int32_t y = sinfo[6]; y < sinfo[8]; y ++)
		{
			const int32_t l = y * w;
			for (int32_t x = sinfo[5]; x < sinfo[7]; x ++)
			{
				int32_t v = data[l + x];
				CDS_COL[x % 8] += v - avg;
				CDS_CNT[x % 8] ++;
			}
		}
	
		int32_t CDS_MIN = 16384;
		for (int32_t i = 0; i < 8; i ++)
		{
			CDS_COL[i] /= CDS_CNT[i];
			if (CDS_MIN > CDS_COL[i]) CDS_MIN = CDS_COL[i];
		}
		
		printf("ADC Column bias: ");
		for (int32_t i = 0; i < 8; i ++)
		{
			CDS_COL[i] -= CDS_MIN;
			printf("%d ", CDS_COL[i]);
			fprintf(fp, "%d ", CDS_COL[i]);
		}
		printf("\n");
	}
	 	
 	delete src;
 	fclose(fp);

	return 0;   
}

