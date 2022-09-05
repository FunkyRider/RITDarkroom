#pragma once

#include <string>
#include "EDSDK.h"
#include "EDSDKErrors.h"
#include "EDSDKTypes.h"

using namespace std;

class CR2Info
{
public:
    CR2Info();
    ~CR2Info();

    int     open(string path);
    int     save();
    void    close();

    string  getExif();
    string  getAdjustments();
    int     setAdjustment(string param, string value);
    int     resetToCapture();

    int     width();
    int     height();
    int     getFull(void* ptr);
    int     getScaled(void* ptr, int dw, int dh);
    int     getRegion(void* ptr, int x, int y, int w, int h);

    static void     ReadVRD(const char *name, unsigned int &vpos, unsigned int &vlen);
    static void     terminate();
private:
    int     _w, _h;
    EdsStreamRef    _cr2;
    EdsImageRef     _img;
    bool            _memFix;
    bool            _noStyle;
    bool            _noCCS;
    static bool     _sdkInited;
};