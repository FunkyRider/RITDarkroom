#include "cr2conv.h"
#include "cr2hot.h"
#include "cr2develop.h"

void my_log_print(const char* log)
{
	
}

void show_help()
{
    printf("cr2raw -c | -j | -t | -h | -d [src_raw] [dest_image]\n");
    
    printf("    -c :  Converts CR2/RW2 raw image to RIT compatible raw image\n");
    printf("    -j :  Develops RIT compatible raw image to JPEG image\n");
    printf("    -t :  Develops RIT compatible raw image to 16-bit TIFF image\n");
	printf("    -h :  Generate hot pixel map or ADC column bias map\n"
			"          To Generate hot pixel map using a dark frame:\n"
			"          Take a shot with body cap on, using parameters:\n"
			"              ISO 1600, 1/10s\n"
			"          To Generate ADC column bias map using a dark frame:\n"
			"          Take a shot with body cap on, using parameters:\n"
			"              ISO 100, 1/1000s\n");
	printf("    -d :  Duplicate VRD (settings) from 1 CR2 to another\n");
}

void show_profile()
{
	printf("Supported cameras:\n");
	
	for (uint32_t i = 0; i < g_camInfo.size(); i ++)
	{
		    printf("    %s\n", g_camInfo[i]->model);
	}

    printf("\nCorrected lenses:\n");

	for (uint32_t i = 0; i < g_lensInfo.size(); i ++)
	{
		printf("    %s\n", (g_lensInfo[i]->dispName[0] != 0) ? g_lensInfo[i]->dispName : g_lensInfo[i]->model);
	}
}

int main(int argc, char** argv)
{
    printf("CR2RAW version v1.01 Alpha - 20150404\n");
    
	if (argc == 2 && argv[1][0] == '-' && argv[1][1] == 'p')
	{
		CR2_InitProfile();
		show_profile();
		return 0;
	}
	else if (argc < 3)
	{
		show_help();
		return 0;
	}
	
	switch (argv[1][1])
	{
	case 'c':
		{
			CR2C_Info info;
			
			info.color_mat = true;
			info.ca_correct = true;
			info.raw_nr = true;
			
			CR2_InitProfile();
			CR2_Convert(&info, argv[2], (argc > 3) ? argv[3] : 0);
		}
		break;
	case 'j':
	case 't':
		{
			CR2D_Info info;
			
			info.tiff = (argv[1][1] == 't');
			info.distortion = true;
			info.chroma_nr = true;
			info.luma_nr = false;
			
			CR2_InitProfile();
			CR2_Develop(&info, argv[2], (argc > 3) ? argv[3] : 0);
		}
		break;
	case 'h':
		CR2_Hot(argv[2]);
		break;
	case 'd':
		{
			CR2Raw src(argv[2]);
			size_t pos, len;
			src.saveVRD(argv[3], &pos, &len);
			printf("VRD Written at 0x%08lX, %lu bytes\n", pos, len);	
		}
		break;
	default:
		show_help();
	}
	
	return 0;
}
