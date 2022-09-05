struct CR2D_Info
{
	bool tiff;
	bool distortion;
	bool chroma_nr;
	bool luma_nr;
};

int CR2_Develop(CR2D_Info* info, const char* fsrc, const char* fdst = 0);
