#include "CR2Info.h"
#include <sstream>
#include <iostream>
#include <iomanip>

using namespace std;

bool CR2Info::_sdkInited = false;

CR2Info::CR2Info()
    : _img(NULL)
    , _cr2(NULL)
{
    if (!_sdkInited)
    {
        EdsInitializeSDK();
        _sdkInited = true;
    }

    _memFix = false;
    _noStyle = false;
    _noCCS = false;
}

CR2Info::~CR2Info()
{
    close();
}

void CR2Info::terminate()
{
    if (_sdkInited)
    {
        EdsTerminateSDK();
        _sdkInited = false;
    }
}

int CR2Info::open(string path)
{
    _memFix = false;
    _noStyle = false;
    _noCCS = false;

    EdsError err = EdsCreateFileStream(path.c_str(), kEdsFileCreateDisposition_OpenExisting, kEdsAccess_ReadWrite, &_cr2);
    if (err != EDS_ERR_OK)
        return err;

    err = EdsCreateImageRef(_cr2, &_img);
    if (err != EDS_ERR_OK)
        return err;

    EdsImageInfo info;
    EdsGetImageInfo(_img, kEdsImageSrc_RAWThumbnail , &info);
    EdsGetImageInfo(_img , kEdsImageSrc_RAWFullView , &info);

    _w = info.width;
    _h = info.height;

    char data_s[128];
    EdsGetPropertyData(_img, kEdsPropID_ProductName, 0, sizeof(data_s), data_s);
    
    if (strcmp(data_s, "EOS-1Ds Mark III") == 0)
    {
        _memFix = true;
    }

    if (strcmp(data_s, "EOS-1D") == 0)
    {
        _noCCS = true;
    }

    EdsUInt32 val32 = 0x7fffffff;
    EdsGetPropertyData(_img, kEdsPropID_PictureStyle, 0, sizeof(val32), &val32);    
    _noStyle = (val32 == 0x7fffffff);

    return 0;
}

int CR2Info::save()
{
    EdsError err = EdsReflectImageProperty(_img);
    return err;
}

void CR2Info::close()
{
    if (_img != NULL)
    {
        EdsRelease(_img);
        EdsRelease(_cr2);
    }

    _img = NULL;
    _cr2 = NULL;
}

string CR2Info::getExif()
{
    stringstream ss;
    char data_s[256];
    EdsImageInfo imgInfo;
    EdsTime imgTime;
    EdsRational rational;
    EdsUInt32 val32;


    EdsGetPropertyData(_img, kEdsPropID_MakerName, 0, sizeof(data_s), data_s);
    ss << "Brand: " << data_s << endl;

    EdsGetPropertyData(_img, kEdsPropID_ProductName, 0, sizeof(data_s), data_s);
    ss << "Model: " << data_s << endl;

    EdsGetImageInfo(_img, kEdsImageSrc_RAWThumbnail , &imgInfo);
    ss << "Thumbnail: " << imgInfo.width << " x " << imgInfo.height << ", " << imgInfo.componentDepth << " bits" << endl;

    EdsGetImageInfo(_img, kEdsImageSrc_RAWFullView , &imgInfo);
    ss << "Dimension: " << imgInfo.width << " x " << imgInfo.height << ", " << imgInfo.componentDepth << " bits" << endl;

    EdsGetPropertyData(_img, kEdsPropID_DateTime, 0, sizeof(imgTime), &imgTime);
    ss << "Time: "  << imgTime.year << "-" 
                    << setfill('0') << setw(2) << imgTime.month << "-"
                    << imgTime.day << " "
                    << imgTime.hour << ":"
                    << setfill('0') << setw(2) << imgTime.minute << setw(-1) << endl;

    EdsGetPropertyData(_img, kEdsPropID_Tv, 0, sizeof(rational), &rational);
    ss << "Shutter: " << rational.numerator << "/" << rational.denominator << "s" << endl;

    EdsGetPropertyData(_img, kEdsPropID_Av, 0, sizeof(rational), &rational);
    ss << "Aperture: F" << std::setprecision(2) << ((double)rational.numerator / rational.denominator) << endl;

    EdsGetPropertyData(_img, kEdsPropID_ExposureCompensation, 0, sizeof(rational), &rational);
    ss << "Compensation: " << std::setprecision(2) << ((double)rational.numerator / rational.denominator) << endl;

    EdsGetPropertyData(_img, kEdsPropID_ISOSpeed, 0, sizeof(val32), &val32);
    ss << "ISO: " << val32 << endl;

    EdsGetPropertyData(_img, kEdsPropID_FocalLength, 0, sizeof(rational), &rational);
    ss << "FocalLength: " << std::setprecision(3) << ((double)rational.numerator / rational.denominator) << "mm" << endl;

    data_s[0] = 0;
    EdsGetPropertyData(_img, kEdsPropID_LensName , 0 , sizeof(data_s), data_s);
    ss << "LensName: " << data_s << endl;

    EdsGetPropertyData(_img, kEdsPropID_BodyID, 0, sizeof(val32), &val32);
    ss << "BodyID: " << setw(10) << val32 << endl;

    EdsGetPropertyData(_img, kEdsPropID_OwnerName , 0 , sizeof(data_s), data_s);
    ss << "OwnerName: " << data_s << endl;

    EdsGetPropertyData(_img, kEdsPropID_FirmwareVersion , 0 , sizeof(data_s), data_s);
    ss << "Firmware: " << data_s << endl;

    return ss.str().c_str();
}

string CR2Info::getAdjustments()
{
    stringstream ss;
    EdsRational rational;
    EdsUInt32 val32 = 0;
    EdsInt32 vali32 = 0;
    EdsInt32 valpair32[2] = {0, 0};
    EdsPictureStyleDesc pstyledesc;

    EdsGetPropertyData(_img, kEdsPropID_DigitalExposure, 0, sizeof(rational), &rational);
    ss << "Exposure: " << setprecision(2) << ((double)rational.numerator / rational.denominator) << endl;

    EdsGetPropertyData(_img, kEdsPropID_WhiteBalance , 0, sizeof(val32), &val32);
    ss << "WhiteBalance: " << val32 << endl;

    if (!_noCCS)
    {
        EdsGetPropertyData(_img, kEdsPropID_WhiteBalanceShift, 0, sizeof(valpair32), valpair32);
        ss << "WBShift: " << valpair32[0] << "," << valpair32[1] << endl;
    }

    EdsGetPropertyData(_img, kEdsPropID_ColorTemperature, 0, sizeof(val32), &val32);
    ss << "ColorTemperature: " << val32 << endl;

    if (_noStyle)
    {
        EdsGetPropertyData(_img, kEdsPropID_ColorMatrix, 0, sizeof(val32), &val32);
        ss << "ColorMatrix: " << val32 << endl;

        if (!_noCCS)
        {
            EdsGetPropertyData(_img, kEdsPropID_Contrast, 0, sizeof(vali32), &vali32);
            ss << "Contrast: " << vali32 << endl;
        }

        EdsGetPropertyData(_img, kEdsPropID_Sharpness, 0, sizeof(valpair32), valpair32);
        ss << "Sharpness: " << valpair32[0] << endl;

        if (!_noCCS)
        {
            EdsGetPropertyData(_img, kEdsPropID_ColorTone, 0, sizeof(vali32), &vali32);
            ss << "ColorTone: " << vali32 << endl;

            EdsGetPropertyData(_img, kEdsPropID_ColorSaturation, 0, sizeof(vali32), &vali32);
            ss << "Saturation: " << vali32 << endl;
        }
    }
    else
    {
        EdsGetPropertyData(_img, kEdsPropID_PictureStyle, 0, sizeof(val32), &val32);
        ss << "PictureStyle: " << val32 << endl;

        EdsGetPropertyData(_img, kEdsPropID_PictureStyleDesc, 0, sizeof(pstyledesc), &pstyledesc);
        ss << "Contrast: " << pstyledesc.contrast << endl;
        ss << "Sharpness: " << pstyledesc.sharpness << endl;
        ss << "ColorTone: " << pstyledesc.colorTone << endl;
        ss << "Saturation: " << pstyledesc.saturation << endl;
        ss << "FilterEffect: " << pstyledesc.filterEffect << endl;
        ss << "ToningEffect: " << pstyledesc.toningEffect << endl;
    }

    EdsGetPropertyData(_img, kEdsPropID_ColorSpace, 0, sizeof(val32), &val32);
    ss << "ColorSpace: " << val32 << endl;

    if (!_noStyle)
    {
        EdsGetPropertyData(_img, kEdsPropID_NoiseReduction, 0, sizeof(val32), &val32);
        ss << "NoiseReduction: " << val32 << endl;
    }

    return ss.str().c_str();
}

int CR2Info::setAdjustment(string param, string value)
{
    EdsRational rational;
    EdsInt32 vali32;
    EdsUInt32 val32;
    EdsInt32 valpair32[2];
    EdsPictureStyleDesc pstyledesc;

    if (param.compare("Exposure") == 0)
    {
        double d = atof(value.c_str());
        rational.numerator = (int)(d * 10);
        rational.denominator = 10;
        EdsSetPropertyData(_img, kEdsPropID_DigitalExposure, 0, sizeof(rational), &rational);
    }
    else if (param.compare("WhiteBalance") == 0)
    {
        val32 = atoi(value.c_str());
        EdsSetPropertyData(_img, kEdsPropID_WhiteBalance, 0, sizeof(val32), &val32);
    }
    else if (param.compare("WBShift") == 0)
    {
        size_t c = value.find(',');
        string s1 = value.substr(0, c);
        string s2 = value.substr(c + 1);
        valpair32[0] = atoi(s1.c_str());
        valpair32[1] = atoi(s2.c_str());
        EdsSetPropertyData(_img, kEdsPropID_WhiteBalanceShift, 0, sizeof(valpair32), valpair32);
        EdsGetPropertyData(_img, kEdsPropID_WhiteBalance , 0, sizeof(val32), &val32);
        EdsSetPropertyData(_img, kEdsPropID_WhiteBalance, 0, sizeof(val32), &val32);
    }
    else if (param.compare("ColorTemperature") == 0)
    {
        val32 = atoi(value.c_str());
        EdsSetPropertyData(_img, kEdsPropID_ColorTemperature, 0, sizeof(val32), &val32);
    }
    else if (param.compare("PictureStyle") == 0 && !_noStyle)
    {
        val32 = atoi(value.c_str());
        EdsSetPropertyData(_img, kEdsPropID_PictureStyle , 0, sizeof(val32), &val32);
    }
    else if (param.compare("Contrast") == 0)
    {
        if (_noStyle)
        {
            vali32 = atoi(value.c_str());
            EdsSetPropertyData(_img, kEdsPropID_Contrast, 0, sizeof(vali32), &vali32);
        }
        else
        {
            EdsGetPropertyData(_img, kEdsPropID_PictureStyleDesc, 0, sizeof(pstyledesc), &pstyledesc);
            pstyledesc.contrast = atoi(value.c_str());
            EdsSetPropertyData(_img, kEdsPropID_PictureStyleDesc, 0, sizeof(pstyledesc), &pstyledesc);
        }
    }
    else if (param.compare("Sharpness") == 0)
    {
        if (_noStyle)
        {
            EdsGetPropertyData(_img, kEdsPropID_Sharpness, 0, sizeof(valpair32), valpair32);
            valpair32[0] = atoi(value.c_str());
            EdsSetPropertyData(_img, kEdsPropID_Sharpness, 0, sizeof(valpair32), valpair32);
        }
        else
        {
            EdsGetPropertyData(_img, kEdsPropID_PictureStyleDesc, 0, sizeof(pstyledesc), &pstyledesc);
            pstyledesc.sharpness = atoi(value.c_str());
            EdsSetPropertyData(_img, kEdsPropID_PictureStyleDesc, 0, sizeof(pstyledesc), &pstyledesc);
        }
    }
    else if (param.compare("ColorTone") == 0)
    {
        if (_noStyle)
        {
            vali32 = atoi(value.c_str());
            EdsSetPropertyData(_img, kEdsPropID_ColorTone, 0, sizeof(vali32), &vali32);
        }
        else
        {
            EdsGetPropertyData(_img, kEdsPropID_PictureStyleDesc, 0, sizeof(pstyledesc), &pstyledesc);
            pstyledesc.colorTone = atoi(value.c_str());
            EdsSetPropertyData(_img, kEdsPropID_PictureStyleDesc, 0, sizeof(pstyledesc), &pstyledesc);
        }
    }
    else if (param.compare("Saturation") == 0)
    {
        if (_noStyle)
        {
            vali32 = atoi(value.c_str());
            EdsSetPropertyData(_img, kEdsPropID_ColorSaturation, 0, sizeof(vali32), &vali32);
        }
        else
        {
            EdsGetPropertyData(_img, kEdsPropID_PictureStyleDesc, 0, sizeof(pstyledesc), &pstyledesc);
            pstyledesc.saturation = atoi(value.c_str());
            EdsSetPropertyData(_img, kEdsPropID_PictureStyleDesc, 0, sizeof(pstyledesc), &pstyledesc);
        }
    }
    else if (param.compare("FilterEffect") == 0 && !_noStyle)
    {
        EdsGetPropertyData(_img, kEdsPropID_PictureStyleDesc, 0, sizeof(pstyledesc), &pstyledesc);
        pstyledesc.filterEffect = atoi(value.c_str());
        EdsSetPropertyData(_img, kEdsPropID_PictureStyleDesc, 0, sizeof(pstyledesc), &pstyledesc);
    }
    else if (param.compare("ToningEffect") == 0 && !_noStyle)
    {
        EdsGetPropertyData(_img, kEdsPropID_PictureStyleDesc, 0, sizeof(pstyledesc), &pstyledesc);
        pstyledesc.toningEffect = atoi(value.c_str());
        EdsSetPropertyData(_img, kEdsPropID_PictureStyleDesc, 0, sizeof(pstyledesc), &pstyledesc);
    }
    else if (param.compare("ColorSpace") == 0)
    {
        val32 = atoi(value.c_str());
        EdsSetPropertyData(_img, kEdsPropID_ColorSpace, 0, sizeof(val32), &val32);
    }
    else if (param.compare("NoiseReduction") == 0)
    {
        val32 = atoi(value.c_str());
        EdsSetPropertyData(_img, kEdsPropID_NoiseReduction, 0, sizeof(val32), &val32);
    }
    else if (param.compare("ColorMatrix") == 0 && _noStyle)
    {
        val32 = atoi(value.c_str());
        EdsSetPropertyData(_img, kEdsPropID_ColorMatrix, 0, sizeof(val32), &val32);
    }
    else
    {
        return -1;
    }

    return 0;
}

int CR2Info::resetToCapture()
{
    stringstream ss;
    EdsRational rational;
    EdsUInt32 val32;
    EdsInt32 valpair32[2];
    EdsPictureStyleDesc pstyledesc;

    EdsGetPropertyData(_img, kEdsPropID_AtCapture_Flag | kEdsPropID_DigitalExposure, 0, sizeof(rational), &rational);
    EdsSetPropertyData(_img, kEdsPropID_DigitalExposure, 0, sizeof(rational), &rational);

    EdsGetPropertyData(_img, kEdsPropID_AtCapture_Flag | kEdsPropID_WhiteBalance , 0, sizeof(val32), &val32);
    EdsSetPropertyData(_img, kEdsPropID_WhiteBalance , 0, sizeof(val32), &val32);

    EdsGetPropertyData(_img, kEdsPropID_AtCapture_Flag | kEdsPropID_WhiteBalanceShift, 0, sizeof(valpair32), valpair32);
    EdsSetPropertyData(_img, kEdsPropID_WhiteBalanceShift, 0, sizeof(valpair32), valpair32);

    EdsGetPropertyData(_img, kEdsPropID_AtCapture_Flag | kEdsPropID_ColorTemperature, 0, sizeof(val32), &val32);
    EdsSetPropertyData(_img, kEdsPropID_ColorTemperature, 0, sizeof(val32), &val32);

    EdsGetPropertyData(_img, kEdsPropID_AtCapture_Flag | kEdsPropID_PictureStyle, 0, sizeof(val32), &val32);
    EdsSetPropertyData(_img, kEdsPropID_PictureStyle, 0, sizeof(val32), &val32);

    EdsGetPropertyData(_img, kEdsPropID_AtCapture_Flag | kEdsPropID_PictureStyleDesc, 0, sizeof(pstyledesc), &pstyledesc);
    EdsSetPropertyData(_img, kEdsPropID_PictureStyleDesc, 0, sizeof(pstyledesc), &pstyledesc);

    EdsGetPropertyData(_img, kEdsPropID_AtCapture_Flag | kEdsPropID_ColorSpace, 0, sizeof(val32), &val32);
    EdsSetPropertyData(_img, kEdsPropID_ColorSpace, 0, sizeof(val32), &val32);

    EdsGetPropertyData(_img, kEdsPropID_AtCapture_Flag | kEdsPropID_NoiseReduction, 0, sizeof(val32), &val32);
    EdsSetPropertyData(_img, kEdsPropID_NoiseReduction, 0, sizeof(val32), &val32);

    return 0;
}

int CR2Info::width()
{
    return _w;
}

int CR2Info::height()
{
    return _h;
}

int CR2Info::getFull(void* ptr)
{
    EdsStreamRef rgb;
    EdsRect imgRect;
    EdsSize imgSize;
    imgRect.point.x = 0;
    imgRect.point.y = 0;
    imgRect.size.width = _w;
    imgRect.size.height = _h;
    imgSize.width = _w;
    imgSize.height = _h;

    //EdsBool value = 1;
    //EdsSetPropertyData(_img, kEdsPropID_Linear, 0, sizeof(value), &value);

    _memFix = false;

    EdsCreateMemoryStream(_memFix ? _w * _h * 6 / 2 : _w * _h * 6, &rgb);
    EdsError err = EdsGetImage(_img, kEdsImageSrc_RAWFullView, kEdsTargetImageType_RGB16, imgRect, imgSize, rgb);
    EdsVoid* sptr;
	EdsGetPointer(rgb, &sptr);
    memcpy(ptr, sptr, _w * _h * 6);
    EdsRelease(rgb);

    return err;
}

int CR2Info::getScaled(void* ptr, int dw, int dh)
{
    EdsStreamRef rgb;
    EdsRect imgRect;
    EdsSize imgSize;
    imgRect.point.x = 0;
    imgRect.point.y = 0;
    imgRect.size.width = _w;
    imgRect.size.height = _h;
    imgSize.width = dw;
    imgSize.height = dh;

    EdsCreateMemoryStreamFromPointer(ptr, dw * dh * 6, &rgb);
    EdsError err = EdsGetImage(_img, kEdsImageSrc_RAWFullView, kEdsTargetImageType_RGB16, imgRect, imgSize, rgb);
    EdsRelease(rgb);

    return err;
}

int CR2Info::getRegion(void* ptr, int x, int y, int w, int h)
{
    EdsStreamRef rgb;
    EdsRect imgRect;
    EdsSize imgSize;
    imgRect.point.x = x;
    imgRect.point.y = y;
    imgRect.size.width = w;
    imgRect.size.height = h;
    imgSize.width = w;
    imgSize.height = h;

    EdsCreateMemoryStreamFromPointer(ptr, w * h * 6, &rgb);
    EdsError err = EdsGetImage(_img, kEdsImageSrc_RAWFullView, kEdsTargetImageType_RGB16, imgRect, imgSize, rgb);
    EdsRelease(rgb);

    return err;
}

void CR2Info::ReadVRD(const char *name, unsigned int &vpos, unsigned int &vlen)
{
    const char* VRD_SIGNATURE = "CANON OPTIONAL DATA";
    FILE* fp = NULL;
    vpos = 0;
    vlen = 0;

    fopen_s(&fp, name, "rb");
    fseek(fp, -0x40, SEEK_END);
    char footer[0x40];
    char header[0x1c];

    fread(footer, 0x40, 1, fp);
    if (strncmp(VRD_SIGNATURE, footer, 19) != 0)
    {
        fclose(fp);
        return;
    }

    char svrdlen[4] = {footer[23], footer[22], footer[21], footer[20]};
    int vrdlen = *((int *) svrdlen);

    fseek(fp, -vrdlen - 0x5c, SEEK_CUR);
    unsigned int vrdpos = ftell(fp);
    fread(header, 0x1c, 1, fp);

    if (strncmp(VRD_SIGNATURE, header, 19) != 0)
    {
        fclose(fp);
        return;
    }

    vpos = vrdpos;
    vlen = vrdlen + 0x5c;

    fclose(fp);
}