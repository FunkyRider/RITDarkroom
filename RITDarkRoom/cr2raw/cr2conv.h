#include "cr2plugins.h"

struct CR2C_Info
{
	bool color_mat;
	bool ca_correct;
	bool raw_nr;
};

extern Array<LensInfo*>		g_lensInfo;
extern Array<CamInfo*>		g_camInfo;

void CR2_InitProfile();
int CR2_Convert(CR2C_Info* info, const char* fsrc, const char* fdst, bool fluorscent_light);
