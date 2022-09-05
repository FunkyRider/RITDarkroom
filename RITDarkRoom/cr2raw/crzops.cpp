#include "crzops.h"
#include "os.h"
#include <io.h>

extern "C" {
#ifdef WIN32
#include "../libzip/zip.h"
#else
#include "zip.h"
#endif
}

int CRZ_SaveStream(CR2Raw* src, const char* fzip)
{
    char tmp[512];
    getTempFile(tmp, 512);
    src->saveRawData(tmp);
    int err = 0;
    
    FILE* fp = _fopen(tmp, "rb");
    char tzip[512];
    getTempFile(tzip, 512);
    deleteFile(tzip);
    zip* z = zip_open(tzip, ZIP_CREATE, &err);

	if (!z)
	{
		printf("Error: cannot create file: %s\n", tzip);
		deleteFile(tzip);
		return -1;
	}

    zip_source* s = zip_source_filep(z, fp, 0, 0);
    int res = zip_add(z, "1.ljpeg", s); 
    zip_close(z);

    fclose(fp);
    deleteFile(tmp);
    moveFile(tzip, fzip);

    return 0;
}

size_t CRZ_read_zip(void* sp, void* buf, size_t len)
{
	zip_file* zf = (zip_file*) sp;
	return zip_fread(zf, buf, len);
}

int CRZ_CreateCR2(const char* fcr2, const char* fzip, const char* ftmp)
{
	bool isAnsi = isStringAnsi(fzip);
    char tmp_zip[512];
	
	if (isAnsi)
	{
		strcpy(tmp_zip, fzip);
	}
	else
	{
		getTempFile(tmp_zip, 512);
		copyFile(fzip, tmp_zip);
	}

    int err = 0;
    zip* z = zip_open(tmp_zip, 0, &err);
    if (err != 0)
    {
        printf("Error: cannot access CRZ file: %s\n", tmp_zip);
        if (!isAnsi) deleteFile(tmp_zip);
        return -1;
    }

    zip_file* zf = zip_fopen(z, "1.ljpeg", 0);
    if (!zf)
    {
        printf("Error: cannot find slice data in: %s\n", fzip);
        zip_close(z);
        if (!isAnsi) deleteFile(tmp_zip);
        return -1;
    }

	StreamReader sr;

	sr.ptr = zf;
	sr.read = CRZ_read_zip;

    CR2Raw src(fcr2);
    src.save(ftmp, 0, &sr);

    zip_fclose(zf);
    zip_close(z);

	if (!isAnsi) deleteFile(tmp_zip);

    return 0;
}