#ifndef _CRZ_OPS_H
#define _CRZ_OPS_H
#include "cr2raw.h"


int CRZ_SaveStream(CR2Raw* src, const char* buf);
int CRZ_CreateCR2(const char* fcr2, const char* fzip, const char* ftmp);

#endif