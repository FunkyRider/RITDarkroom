#include "os.h"

// File name is in UTF-8 encoding
FILE* _fopen(const char* fname, const char* mode)
{
#ifdef _WIN32
    int flen = MultiByteToWideChar(CP_UTF8, 0, fname, -1, NULL, 0);
    wchar_t *wfname = new wchar_t[flen];
    MultiByteToWideChar(CP_UTF8, 0, fname, -1, (LPWSTR) wfname, flen);

    int mlen = MultiByteToWideChar(CP_UTF8, 0, mode, -1, NULL, 0);
    wchar_t *wmode = new wchar_t[mlen];
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, (LPWSTR) wmode, mlen);

    FILE* fp = _wfopen(wfname, wmode);

    delete[] wfname;
    delete[] wmode;

    return fp;
#else
    return fopen(fname, mode);
#endif
}

const char* getAppPath()
{
    static char appPath[512];

    if (appPath[0])
        return appPath;

#ifdef _WIN32
    {
        wchar_t buf[512];
	    GetModuleFileNameW(NULL, buf, _COUNT(buf));
        WideCharToMultiByte(CP_UTF8, 0, buf, -1, appPath, _COUNT(appPath), NULL, NULL);
    }
#else
	if (readlink("/proc/self/exe", appPath, _COUNT(appPath)) < 0)
		strcpy(appPath, "./");
#endif
	int32_t psize = strlen(appPath);    
	for (int i = psize; i > 0; i --)
	{
		if (appPath[i] == '\\' || appPath[i] == '/')
			break;
		appPath[i] = 0;
	}

    return appPath;
}

static wchar_t _g_temp_path[4096] = L"\0";

void setTempPath(const wchar_t* path)
{
	wcscpy_s(_g_temp_path, path);
}

int getTempFile(char* buf, int fsize)
{
    int len = 0;

#ifdef _WIN32
    wchar_t pbuf[512], tname[512];

	if (_g_temp_path[0] != 0)
	{
		wcscpy_s(pbuf, _g_temp_path);
	}
	else
	{
		GetTempPath(512, pbuf);
	}

    GetTempFileName(pbuf, L"_RIT_", 0, tname);
    len = WideCharToMultiByte(CP_UTF8, 0, tname, -1, buf, fsize, NULL, NULL);
#else
    strcpy(buf, "/tmp/_RIT_");
    for (int i = 0; i < 16; i ++)
    {
        buf[i + 10] = 'a' + (rand() % 26);
    }
    buf[26] = 0;
    len = 26;
#endif

    return len;
}

int deleteFile(const char* fname)
{
    int res = 0;

#ifdef _WIN32
    int flen = MultiByteToWideChar(CP_UTF8, 0, fname, -1, NULL, 0);
    wchar_t *wfname = new wchar_t[flen];
    MultiByteToWideChar(CP_UTF8, 0, fname, -1, (LPWSTR) wfname, flen);
    res = DeleteFile(wfname);
    delete[] wfname;
#else
    res = remove(fname);
#endif

    return res;
}

int copyFile(const char* from, const char* to)
{
    int res = 0;

#ifdef _WIN32
    int flen = MultiByteToWideChar(CP_UTF8, 0, from, -1, NULL, 0);
    wchar_t *wfrom = new wchar_t[flen];
    MultiByteToWideChar(CP_UTF8, 0, from, -1, (LPWSTR) wfrom, flen);
    
    int tlen = MultiByteToWideChar(CP_UTF8, 0, to, -1, NULL, 0);
    wchar_t *wto = new wchar_t[tlen];
    MultiByteToWideChar(CP_UTF8, 0, to, -1, (LPWSTR) wto, tlen);
    
    res = CopyFile(wfrom, wto, FALSE) ? 0 : -1;

    
    delete[] wfrom;
    delete[] wto;
#else
    const int BUFSIZ = 16384;
    char buf[BUFSIZ];
    size_t size;

    int source = open(from, O_RDONLY, 0);
    int dest = open(to, O_WRONLY | O_CREAT /*| O_TRUNC/**/, 0644);

    while ((size = read(source, buf, BUFSIZ)) > 0) {
        write(dest, buf, size);
    }

    close(source);
    close(dest);
#endif

    return res;
}

int moveFile(const char* from, const char* to)
{
    int res = 0;

#ifdef _WIN32
    int flen = MultiByteToWideChar(CP_UTF8, 0, from, -1, NULL, 0);
    wchar_t *wfrom = new wchar_t[flen];
    MultiByteToWideChar(CP_UTF8, 0, from, -1, (LPWSTR) wfrom, flen);
    
    int tlen = MultiByteToWideChar(CP_UTF8, 0, to, -1, NULL, 0);
    wchar_t *wto = new wchar_t[tlen];
    MultiByteToWideChar(CP_UTF8, 0, to, -1, (LPWSTR) wto, tlen);
    
    bool r = MoveFile(wfrom, wto) ? true : false;

    if (!r)
    {
        if (CopyFile(wfrom, wto, FALSE))
        {
            DeleteFile(wfrom);
            res = 0;
        }
        else
        {
            res = -1;
        }
    }
    else
    {
        res = 0;
    }
    
    delete[] wfrom;
    delete[] wto;
#else
    res = rename(from, to);
#endif

    return res;
}

bool isStringAnsi(const char* str)
{
	for (const char* ch = str; *ch != '0'; ch ++)
	{
		if (*ch > 127)
			return false;
	}

	return true;
}