#ifndef _OS_H
#define _OS_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define _COUNT(p) (sizeof (p) / sizeof (p[0]))

FILE*           _fopen(const char* fname, const char* mode);
const char*     getAppPath();
int             getTempFile(char* buf, int fsize);
int             deleteFile(const char* fname);
int             moveFile(const char* from, const char* to);
int             copyFile(const char* from, const char* to);
bool			isStringAnsi(const char* str);
void			setTempPath(const wchar_t* path);

typedef size_t (StreamReader_read_t)(void *sp, void* buf, size_t len);

struct StreamReader
{
	void*					ptr;
	StreamReader_read_t*	read;
};
#endif