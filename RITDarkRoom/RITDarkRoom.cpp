// RITDarkRoom.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "RITDarkRoom.h"
#include <commctrl.h>
#include <commdlg.h>
#include <string>
#include <shlobj.h>
#include <iostream>
#include <sstream>

#include "cr2conv.h"
#include "cr2hot.h"
#include "cr2develop.h"

#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <fcntl.h>
#include <array.h>

//#include <omp.h>
#include "cpuinfo.h"
#include "cr2info\\CR2Info.h"
#include "os.h"

#define MYPRINT      1
#define WM_MYMESSAGE (WM_USER+1)

#pragma comment(lib, "Comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

HWND g_hDlg;

//=====================================================================================
//
// Helpers
//
//=====================================================================================

const char* getAppPath();
void redirectStdOut();

bool g_goingImport = false;
bool g_goingDevelop = false;
bool g_stopImport = false;
bool g_stopDevelop = false;

ITaskbarList3* g_pTaskBarlist = NULL;


static int CALLBACK BrowseCallbackProc(HWND hwnd,UINT uMsg, LPARAM lParam, LPARAM lpData)
{

    if(uMsg == BFFM_INITIALIZED)
    {
        //std::string tmp = (const char *) lpData;
        //std::cout << "path: " << tmp << std::endl;
        SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
    }

    return 0;
}

std::wstring browseFolder(std::wstring saved_path)
{
    TCHAR path[MAX_PATH];

    const wchar_t * path_param = saved_path.c_str();

    BROWSEINFO bi = { 0 };
    bi.lpszTitle  = (L"Browse for folder...");
    bi.ulFlags    = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn       = BrowseCallbackProc;
    bi.lParam     = (LPARAM) path_param;

    LPITEMIDLIST pidl = SHBrowseForFolder ( &bi );

    if ( pidl != 0 )
    {
        //get the name of the folder and put it in path
        SHGetPathFromIDList ( pidl, path );

        //free memory used
        IMalloc * imalloc = 0;
        if ( SUCCEEDED( SHGetMalloc ( &imalloc )) )
        {
            imalloc->Free ( pidl );
            imalloc->Release ( );
        }

        return path;
    }

    return L"";
}

std::wstring browseFile(std::wstring saved_path)
{
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    wchar_t fname[MAX_PATH];

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hDlg;
    ofn.lpstrDefExt = L"";
    ofn.lpstrFile = fname;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Canon CR2 Raw\0*.CR2\0Panasonic RW2 Raw\0*.RW2";
    ofn.nFilterIndex = 0;
    ofn.lpstrInitialDir = saved_path.c_str();
    ofn.lpstrTitle = L"Open Raw file";
    ofn.Flags = 0;

    GetOpenFileName(&ofn);

    if (_tcslen(fname) == 0) return L"";

    return fname;

}

std::wstring getText(int id)
{
    wchar_t buf[512];
    GetDlgItemText(g_hDlg, id, buf, _countof(buf));
    return buf;
}

void setText(int id, std::wstring text)
{
    SetDlgItemText(g_hDlg, id, text.c_str());
}

void clearText(int id)
{
    SetDlgItemText(g_hDlg, id, L"");
}

void appendText(int id, std::wstring text)
{
    HWND hEdit = GetDlgItem(g_hDlg, id);
    text += L"\r\n";
    int len = GetWindowTextLength(hEdit);

    SendMessage(hEdit, EM_SETSEL, (WPARAM) len, (LPARAM) len);
    SendMessage(hEdit, EM_REPLACESEL, FALSE, (LPARAM) text.c_str());
}

int pathWalk(std::wstring path, bool create)
{
    return 0;
}

int g_numLines = 0;

void log(std::wstring text)
{
    appendText(IDC_LOG_T1, text);
    SendMessage(GetDlgItem(g_hDlg, IDC_LOG_T1), EM_LINESCROLL, 0, g_numLines ++);
}

void clearLog()
{
    clearText(IDC_LOG_T1);
    g_numLines = 0;
}

HANDLE g_dir = 0;

std::wstring getFirstFile(std::wstring path)
{
    if (g_dir != 0)
        FindClose(g_dir);

    WIN32_FIND_DATA fd;
    g_dir = FindFirstFile(path.c_str(), &fd);

    return fd.cFileName;
}

std::wstring getNextFile()
{
    WIN32_FIND_DATA fd;

    if (g_dir != 0 && FindNextFile(g_dir, &fd))
    {
       return fd.cFileName;
    }

    return L"";
}

void closeDir()
{
    FindClose(g_dir);
    g_dir = 0;
}

// Convert a wide Unicode string to an UTF8 string
std::string utf8_encode(const std::wstring &wstr)
{
    if( wstr.empty() ) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo( size_needed, 0 );
    WideCharToMultiByte                  (CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Convert an UTF8 string to a wide Unicode String
std::wstring utf8_decode(const std::string &str)
{
    if( str.empty() ) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo( size_needed, 0 );
    MultiByteToWideChar                  (CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string readLine(FILE* fp)
{
#define LINESZ 1024
    char buff[LINESZ];
    int len = 0;
    char ch;

    while (len < LINESZ) {
        ch = fgetc(fp);

        if (ch == EOF || ch == 13)
            break;

        if (ch != 10)
        {
            buff[len ++] = ch;
        }
    }

    buff[len] = 0;

    return buff;
}

bool getCheck(int id)
{
    int res = SendMessage(GetDlgItem(g_hDlg, id), BM_GETCHECK, 0, 0);

    return (res == TRUE);
}

void setCheck(int id, bool checked)
{
    int val = checked ? TRUE : FALSE;

    SendMessage(GetDlgItem(g_hDlg, id), BM_SETCHECK, (WPARAM) (val), (LPARAM) (val));
}

std::wstring getFilePath(std::wstring path)
{
    size_t ps = path.find_last_of('\\');
    return path.substr(0, ps);
}

std::wstring getFileName(std::wstring path)
{
    size_t ps = path.find_last_of('\\');

    if (ps != path.npos)
    {
        return path.substr(ps + 1);
    }
    else
    {
        return path;
    }
}

std::wstring getFileNameNoExt(std::wstring path)
{
    size_t ps = path.find_last_of('.');

    if (ps != path.npos)
    {
        return path.substr(0, ps);
    }
    else
    {
        return path;
    }
}

std::wstring getFileExt(std::wstring path)
{
    size_t ps = path.find_last_of('.');

    if (ps != path.npos)
    {
        return path.substr(ps);
    }
    else
    {
        return L"";
    }
}

void enableUI(bool enable, int imp_or_dev)
{
    EnableWindow(GetDlgItem(g_hDlg, IDC_IMPORT_T1), enable);
    EnableWindow(GetDlgItem(g_hDlg, IDC_IMPORT_T2), enable);
    EnableWindow(GetDlgItem(g_hDlg, IDC_IMPORT_B1), enable);
    EnableWindow(GetDlgItem(g_hDlg, IDC_IMPORT_B2), enable);
    EnableWindow(GetDlgItem(g_hDlg, IDC_IMPORT_B3), enable);
    EnableWindow(GetDlgItem(g_hDlg, IDC_IMPORT_C1), enable);
    EnableWindow(GetDlgItem(g_hDlg, IDC_IMPORT_C2), enable);
    EnableWindow(GetDlgItem(g_hDlg, IDC_IMPORT_C3), enable);
    EnableWindow(GetDlgItem(g_hDlg, IDC_IMPORT_C4), enable);

    EnableWindow(GetDlgItem(g_hDlg, IDC_DEVELOP_T1), enable);
    EnableWindow(GetDlgItem(g_hDlg, IDC_DEVELOP_T2), enable);
    EnableWindow(GetDlgItem(g_hDlg, IDC_DEVELOP_B1), enable);
    EnableWindow(GetDlgItem(g_hDlg, IDC_DEVELOP_B2), enable);
    EnableWindow(GetDlgItem(g_hDlg, IDC_DEVELOP_B3), enable);
    EnableWindow(GetDlgItem(g_hDlg, IDC_DEVELOP_C1), enable);
    EnableWindow(GetDlgItem(g_hDlg, IDC_DEVELOP_C2), enable & 0);
    EnableWindow(GetDlgItem(g_hDlg, IDC_DEVELOP_C3), enable);

    EnableWindow(GetDlgItem(g_hDlg, IDC_TOOLS_B1), enable);
    EnableWindow(GetDlgItem(g_hDlg, IDC_TOOLS_B2), enable);

    if (enable)
    {
        SetDlgItemText(g_hDlg, IDC_IMPORT_B3, L"Import");
        SetDlgItemText(g_hDlg, IDC_DEVELOP_B3, L"Develop");
    }
    else
    {
        if (imp_or_dev == 0)
        {
            SetDlgItemText(g_hDlg, IDC_IMPORT_B3, L"STOP");
            EnableWindow(GetDlgItem(g_hDlg, IDC_IMPORT_B3), true);
        }
        else
        {
            SetDlgItemText(g_hDlg, IDC_DEVELOP_B3, L"STOP");
            EnableWindow(GetDlgItem(g_hDlg, IDC_DEVELOP_B3), true);
        }
    }
}

void setProgress(int mode, int id, int total)
{
	wchar_t buf[128];

	switch (mode)
	{
	case 1:
		wsprintf(buf, L"Importing %d of %d - RIT Darkroom", id, total);
		if (g_pTaskBarlist)
		{
			g_pTaskBarlist->SetProgressState(g_hDlg, TBPF_NORMAL);
			g_pTaskBarlist->SetProgressValue(g_hDlg, id, total);
		}
		break;
	case 2:
		wsprintf(buf, L"Developing %d of %d - RIT Darkroom", id, total);
		if (g_pTaskBarlist)
		{
			g_pTaskBarlist->SetProgressState(g_hDlg, TBPF_NORMAL);
			g_pTaskBarlist->SetProgressValue(g_hDlg, id, total);
		}
		break;
	case -1:
		if (g_pTaskBarlist)
		{
			g_pTaskBarlist->SetProgressState(g_hDlg, TBPF_ERROR);
		}
		break;
	default:
		wsprintf(buf, L"RIT Darkroom");
		if (g_pTaskBarlist)
		{
			g_pTaskBarlist->SetProgressState(g_hDlg, TBPF_NOPROGRESS);
		}
		break;
	}

	if (mode >= 0)
	{
		SetWindowText(g_hDlg, buf);
	}
}

//=====================================================================================
//
// Event handlers
//
//=====================================================================================

const int32_t curve[] = 
	{0,2,6,10,14,18,22,26,30,34,38,42,46,50,54,58,62,66,70,74,78,82,86,90,94,98,
	102,106,110,114,118,122,126,130,134,138,142,146,150,154,158,162,166,170,174,
	178,182,186,190,194,198,202,206,210,214,218,222,226,230,234,238,242,246,250,
	254,258,262,266,270,274,278,282,286,290,294,298,304,308,312,316,320,324,328,
	332,336,340,346,350,354,360,364,368,374,378,384,388,394,400,404,410,416,420,
	426,432,438,444,452,458,464,472,478,486,494,500,510,518,526,534,542,552,560,
	568,578,586,596,604,614,624,632,642,652,662,672,682,692,704,714,726,736,748,
	758,770,782,794,806,820,834,846,860,874,888,904,920,936,952,970,988,1004,1022,
	1040,1058,1076,1094,1112,1130,1148,1166,1184,1202,1222,1240,1258,1276,1296,1314,
	1334,1352,1372,1390,1410,1428,1448,1468,1488,1506,1526,1546,1566,1586,1606,1626,
	1646,1668,1688,1708,1730,1752,1772,1794,1816,1840,1862,1886,1908,1932,1956,1982,
	2006,2032,2058,2084,2110,2138,2166,2194,2222,2250,2280,2310,2340,2370,2400,2430,
	2462,2492,2524,2554,2586,2618,2650,2682,2714,2746,2780,2812,2846,2880,2914,2948,
	2982,3018,3052,3088,3124,3160,3196,3232,3270,3306,3344,3382,3420,3460,3498,3538,
	3578,3585}; // 3585 max

const int32_t cam[] = {8016,-1694,-1204,-5796,13887,2230,-1784,1944,7671};
const float xyz_rgb[9] = 			/* XYZ from RGB */
  {0.412453f, 0.357580f, 0.180423f,
  0.212671f, 0.715160f, 0.072169f,
  0.019334f, 0.119193f, 0.950227f};

//const float cam_rgb[] = {2.02f, -0.18f, 0.01f, -1.13f, 1.5f, -0.32f, 0.11f, -0.32f, 1.31f};
const float cam_rgb[] = {1.96f, -0.18f, 0.0f, -1.09f, 1.53f, -0.32f, 0.13f, -0.35f, 1.31f};

//const uint16_t _BLACK = 2047, _SATURATION = 15100;
const uint16_t _BLACK = 127, _SATURATION = 3500;
bool gbgb = true;

#include "saveimage.h"

int test(const char* fin, const char* fout)
{
	printf("Step 1...\n");
	// 1. Load/decode
	CR2Raw* src = new CR2Raw(fin);
	const int32_t w = src->getWidth() - 2, h = src->getHeight() - 2;
    uint16_t* data = src->getRawData();    
    const int32_t s = w * h;
    Image* bayer = new Image(w / 2, h / 2, eImageFormat_RGBG);

    bayer->fromRAW16(data, _SATURATION, _BLACK, 0, 0, w + 2);
    
	printf("Step 2...\n");
    // 2. Demosaic
    uint32_t* wb = src->getWBCoefficients();
    float fwb[3] = {(float) wb[0] / (float) wb[1], 1.0f, (float) wb[3] / (float) wb[1]};
    Image* img = bayer->demosaicRAW(eImageDemosaic_AHD, cam_rgb, fwb);
    delete bayer;

	printf("Step 3...\n");
    // 3. WB scale
    
	printf("Step 4...\n");
    // 4. Color matrix: cam_xyz, xyz_rgb
    //float cam_xyz[9], cam_rgb[9], cam_cam[9];
    
    //for (int32_t i = 0; i < 9; i ++)
    //	cam_xyz[i] = (float) cam[i] / 10000.0f;
    
    //m3x3Multiply(cam_rgb, xyz_rgb, cam_xyz);
    //m3x3Inverse(cam_cam, cam_rgb);

    const float rgb_ycc[] = {0.299f, -0.168736f, 0.5f, 0.587f, -0.331264f, -0.418688f, 0.114f, 0.5f, -0.081312f};
	float cam_ycc[9];
	m3x3Multiply(cam_ycc, rgb_ycc, cam_rgb);

    float* ch[] = {img->channel(0), img->channel(1), img->channel(2)};
#pragma omp parallel for
	for (int32_t i = 0; i < s; i ++)
    {
        const float r = ch[0][i] * fwb[0];
        const float g = ch[1][i];
        const float b = ch[2][i] * fwb[2];

        //const float dr = (r * cam_cam[0] + g * cam_cam[1] + b * cam_cam[2]);
        //const float dg = (r * cam_cam[3] + g * cam_cam[4] + b * cam_cam[5]);
        //const float db = (r * cam_cam[6] + g * cam_cam[7] + b * cam_cam[8]);

        ch[0][i] = r;//(r * cam_rgb[0] + g * cam_rgb[3] + b * cam_rgb[6]);
        ch[1][i] = g;//(r * cam_rgb[1] + g * cam_rgb[4] + b * cam_rgb[7]);
        ch[2][i] = b;//(r * cam_rgb[2] + g * cam_rgb[5] + b * cam_rgb[8]);
	}

	//img->toFormat(eImageFormat_YCbCr);

#pragma omp parallel for
	for (int32_t i = 0; i < s; i ++)
	{
		ch[0][i] = ch[0][i] / 65535.0f * 6700.0f;
        ch[1][i] = ch[1][i] / 65535.0f * 6700.0f;
        ch[2][i] = ch[2][i] / 65535.0f * 6700.0f;
    }
    
	printf("Step 5...\n");
    
    // 5. Tone curve
    uint16_t* rgb12 = new uint16_t[s * 3];
    img->toRGB16(rgb12);
    delete img;
    
    
    uint8_t tcurve[3590];    
    for (int32_t i = 0; i < 3590; i ++)
    {
    	for (int32_t j = 0; j < 256; j ++)
    	{
    		if (i <= curve[j])
    		{
    			tcurve[i] = j;
    			break;
    		}
    	}
    }
    

   uint8_t* rgb8 = new uint8_t[s * 3];

//#pragma omp parallel for
    int max = 0;

    for (int32_t i = 0; i < s * 3; i ++)
    {
    	uint16_t v = rgb12[i];

        if (max < v) max = v;
    	if (v > 3578) v = 3578;
    	
    	int32_t p = tcurve[v];
        //int32_t p = v;
        if (p < 0) p = 0; else if (p > 255) p = 255;
    	rgb8[i] = p;
    }
    
    delete[] rgb12;


    
	printf("Step 6 %d...\n", max);
    // 6. Save tiff
    SaveTIFF(rgb8, w, h, 24, fout);
	delete[] rgb8;
    
    delete src;
    return 0;
}

void measureSat(const char* file)
{
	CR2Raw raw(file);
	int l = raw.getWidth() * raw.getHeight();
	uint16_t* data = raw.getRawData();

	int min = 100000, max = 0;

	for (int i = 0; i < l; i ++)
	{
		uint16_t d = data[i];

		if (min > d) min = d;
		if (max < d) max = d;
	}

	printf("ISO: %d, Black: %d, Saturation: %d\n", raw.getISO(), min, max);
}

std::wstring g_pathT1, g_pathT2;

void on_startup()
{
	//test("D:\\Private\\CR2\\1D2N\\TMP\\E5DhSLI0200.CR2", "D:\\Private\\CR2\\1D2N\\TMP\\E5DhSLI0200.TIF");
	//measureSat("E:\\Photos\\1Ds2_Sat\\239M9346.CR2");
    log(L"RIT Darkroom v0.49b");
    log(L"Author: Bo Zhang");
	log(L"Special thanks to ldsclub.net forum members for testing and suggestions");
	
	int nth = 1;
#pragma omp parallel
    {
#pragma omp single
		{
			nth = omp_get_num_threads();
		}
	}
	{
		wchar_t buf[64];
		wsprintf(buf, L"CPU: Number of threads: %d\n", nth);
		log(buf);
	}
	if (CPUID::hasSSE4_1())
	{
		log(L"CPU: SSE 4.1 acceleration enabled");
	}

	// Default settings
    setCheck(IDC_IMPORT_C1, true);
    setCheck(IDC_IMPORT_C2, true);
    setCheck(IDC_IMPORT_C3, true);
    //setCheck(IDC_DEVELOP_C2, true);
    setCheck(IDC_DEVELOP_C3, true);

    // Load settings
    FILE* fp = NULL;
    std::string path(getAppPath());
    path += "config.txt";
    fopen_s(&fp, path.c_str(), "r");
	
    if (fp)
    {
        setText(IDC_IMPORT_T1, utf8_decode(readLine(fp)).c_str());
        setText(IDC_IMPORT_T2, utf8_decode(readLine(fp)).c_str());

        int ic1 = 1, ic2 = 1, ic3 = 0;

		size_t p = 0;
        String sline(readLine(fp).c_str());

        setCheck(IDC_IMPORT_C1, sline.term(p).toInt() != 0);
        setCheck(IDC_IMPORT_C2, sline.term(p).toInt() != 0);
        setCheck(IDC_IMPORT_C3, sline.term(p).toInt() != 0);
		if (p < sline.length())
			setCheck(IDC_IMPORT_C4, sline.term(p).toInt() != 0);

        setText(IDC_DEVELOP_T1, utf8_decode(readLine(fp)).c_str());
        setText(IDC_DEVELOP_T2, utf8_decode(readLine(fp)).c_str());

        p = 0;
        sline = readLine(fp).c_str();
        setCheck(IDC_DEVELOP_C1, sline.term(p).toInt() != 0);
        setCheck(IDC_DEVELOP_C2, sline.term(p).toInt() != 0);
        setCheck(IDC_DEVELOP_C3, sline.term(p).toInt() != 0);

        fclose(fp);
    }

	setProgress(0, 0, 0);

	CR2_InitProfile();

    wchar_t buf[128];    
    wsprintf(buf, L"Profile: %d cameras loaded\r\nProfile: %d lenses loaded", g_camInfo.size(), g_lensInfo.size());
    log(buf);
}

void on_exit()
{
    // Save settings
    const char* lend = "\r\n";

    FILE* fp = NULL;
    std::string path(getAppPath());
    path += "config.txt";
    fopen_s(&fp, path.c_str(), "w");

    if (!fp)
        return;

    std::string l = utf8_encode(getText(IDC_IMPORT_T1));
    fwrite(l.c_str(), 1, l.length(), fp);
    fwrite(lend, 1, 2, fp);

    l = utf8_encode(getText(IDC_IMPORT_T2));
    fwrite(l.c_str(), 1, l.length(), fp);
    fwrite(lend, 1, 2, fp);

    char lbuf[128];
    int ic1 = getCheck(IDC_IMPORT_C1) ? 1 : 0;
    int ic2 = getCheck(IDC_IMPORT_C2) ? 1 : 0;
    int ic3 = getCheck(IDC_IMPORT_C3) ? 1 : 0;
    int ic4 = getCheck(IDC_IMPORT_C4) ? 1 : 0;
    sprintf_s(lbuf, "%d %d %d %d", ic1, ic2, ic3, ic4);
    size_t len = strlen(lbuf);
    fwrite(lbuf, 1, len, fp);
    fwrite(lend, 1, 2, fp);

    l = utf8_encode(getText(IDC_DEVELOP_T1));
    fwrite(l.c_str(), 1, l.length(), fp);
    fwrite(lend, 1, 2, fp);

    l = utf8_encode(getText(IDC_DEVELOP_T2));
    fwrite(l.c_str(), 1, l.length(), fp);
    fwrite(lend, 1, 2, fp);

    int dc1 = getCheck(IDC_DEVELOP_C1) ? 1 : 0;
    int dc2 = getCheck(IDC_DEVELOP_C2) ? 1 : 0;
    int dc3 = getCheck(IDC_DEVELOP_C3) ? 1 : 0;
    sprintf_s(lbuf, "%d %d %d", dc1, dc2, dc3);
    len = strlen(lbuf);
    fwrite(lbuf, 1, len, fp);
    fwrite(lend, 1, 2, fp);

    fclose(fp);

    CR2Info::terminate();
}

int on_Import(std::wstring file, std::wstring dest_path)
{
    bool opt_colorspace = getCheck(IDC_IMPORT_C1);
    bool opt_cacorrect = getCheck(IDC_IMPORT_C2);
    bool opt_denoise = getCheck(IDC_IMPORT_C3);
	bool opt_fluorscent = getCheck(IDC_IMPORT_C4);

	setTempPath(dest_path.c_str());

    std::string ufile = utf8_encode(file);
    std::string udest = utf8_encode(dest_path);
    std::wstring fname = getFileNameNoExt(getFileName(file));    
    fname += L"_RIT.CR2";
    std::string uname = utf8_encode(fname);
    udest += "\\";
    udest += uname;

    CR2C_Info info;
	
	info.color_mat = opt_colorspace;
	info.ca_correct = opt_cacorrect;
	info.raw_nr = opt_denoise;

	return CR2_Convert(&info, ufile.c_str(), udest.c_str(), opt_fluorscent);
}

int on_Develop(std::wstring file, std::wstring dest_path)
{
    bool opt_tiff = getCheck(IDC_DEVELOP_C1);
    bool opt_distortion = getCheck(IDC_DEVELOP_C2);
    bool opt_denoise = getCheck(IDC_DEVELOP_C3);

	setTempPath(dest_path.c_str());

    std::string ufile = utf8_encode(file);
    std::string udest = utf8_encode(dest_path);
    std::wstring fname = getFileNameNoExt(getFileName(file));    
    fname += (opt_tiff) ? L".TIF" : L".JPG";
    std::string uname = utf8_encode(fname);
    udest += "\\";
    udest += uname;

    CR2D_Info info;
			
	info.tiff = opt_tiff;
	info.distortion = opt_distortion;
	info.chroma_nr = opt_denoise;
	info.luma_nr = false;
	
	return CR2_Develop(&info, ufile.c_str(), udest.c_str());
}

void on_button_Import_Browse1()
{    
    std::wstring path = browseFolder(getText(IDC_IMPORT_T1));
    if (path.length() > 0)
        setText(IDC_IMPORT_T1, path);
}

void on_button_Import_Browse2()
{
    std::wstring path = browseFolder(getText(IDC_IMPORT_T2));
    if (path.length() > 0)
    {
        setText(IDC_IMPORT_T2, path);
        setText(IDC_DEVELOP_T1, path);
    }
}

static HCURSOR hCursorWait = NULL;
static HCURSOR hCursorNorm = NULL;

#define RITD_MT_PROC	1

void on_Import_thread(void* pFiles)
{
	Array<std::wstring> &files = *(static_cast<Array<std::wstring>*> (pFiles));

	const std::wstring &path = files[0];
	const std::wstring &dpath = files[1];

	for (uint32_t i = 2; i < files.size() && !g_stopImport; i ++)
	{
		//setProgress(1, i + 1, files.size());
		log(std::wstring(L"Importing: ") + files[i]);

		int res = on_Import(path + files[i], dpath);
        if (res < 0)
        {
            //err = true;
            break;
        }
        else
        {
            //count += res;

			//if (res == 0)
			//	Sleep(10);
        }
	}
}

void on_button_Import_Go()
{
    g_goingImport = true;

    enableUI(false, 0);

    clearLog();
    std::wstring path = getText(IDC_IMPORT_T1) + L"\\";
    std::wstring dpath = getText(IDC_IMPORT_T2);
    std::wstring file = getFirstFile(path + L"*.*");
    bool err = false;
    int count = 0;
	Array<std::wstring> files;

    while (file.length() > 0)
    {
        size_t l = file.length();
        std::wstring ext = (l > 4) ? file.substr(l - 4) : L"";

        if (_wcsicmp(ext.c_str(), L".CR2") == 0 ||
            _wcsicmp(ext.c_str(), L".RW2") == 0 ||
            _wcsicmp(ext.c_str(), L".RWL") == 0)
        {
            files.add(file);
        }
        file = getNextFile();
    }
	closeDir();

	{
		wchar_t buf[512];
		wsprintf(buf, L"Importing %d images from: %s\n", files.size(), path.c_str());
		log(buf);
	}

#ifdef RITD_MT_PROC
	const uint32_t num_threads = 2;

	Array<std::wstring>*	files_th = new Array<std::wstring>[num_threads];

	for (uint32_t i = 0; i < num_threads; i ++)
	{
		files_th[i].add(path);
		files_th[i].add(dpath);
	}

	int tid = 0;
	for (uint32_t i = 0; i < files.size() && !g_stopImport; i ++)
	{
		files_th[tid].add(files[i]);

		tid ++;
		if (tid >= num_threads)
		{
			tid = 0;
		}
	}

	HANDLE hid[32];
	{
		wchar_t buf[512];
		for (uint32_t i = 0; i < num_threads; i ++)
		{
			DWORD t = 0;

			wsprintf(buf, L"Spawning thread: #%d", i);
			log(std::wstring(buf));
			hid[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) &on_Import_thread, (void *) &files_th[i], 0, &t);
		}
	}

	WaitForMultipleObjects(num_threads, hid, true, INFINITE);

	delete[] files_th;

#else
	for (uint32_t i = 0; i < files.size() && !g_stopImport; i ++)
	{
		setProgress(1, i + 1, files.size());
		log(std::wstring(L"Importing: ") + files[i]);

		int res = on_Import(path + files[i], dpath);
        if (res < 0)
        {
            err = true;
            break;
        }
        else
        {
            count += res;

			if (res == 0)
				Sleep(10);
        }
	}
#endif

    if (g_stopImport)
        log(L"==== Stopped by User ====");

    wchar_t buf[128];
    wsprintf(buf, L"==== %d Images Imported ====", count);

    if (!err)
        log(buf);
    else
        log(L"======== Error ========");

	setProgress(0, 0, 0);
    g_stopImport = false;
    g_goingImport = false;
    enableUI(true, 0);
}

void on_button_Develop_Browse1()
{
    std::wstring path = browseFolder(getText(IDC_DEVELOP_T1));
    if (path.length() > 0)
        setText(IDC_DEVELOP_T1, path);
}

void on_button_Develop_Browse2()
{
    std::wstring path = browseFolder(getText(IDC_DEVELOP_T2));
    if (path.length() > 0)
        setText(IDC_DEVELOP_T2, path);
}

void on_button_Develop_Go()
{
    g_goingDevelop = true;

    enableUI(false, 1);
    clearLog();
    std::wstring path = getText(IDC_DEVELOP_T1) + L"\\";
    std::wstring dpath = getText(IDC_DEVELOP_T2);
    std::wstring file = getFirstFile(path + L"*.*");
    bool err = false;
    int count = 0;
	Array<std::wstring> files;

    while (file.length() > 0)
    {
        size_t l = file.length();
        std::wstring ext = (l > 4) ? file.substr(l - 4) : L"";

        if (_wcsicmp(ext.c_str(), L".CR2") == 0)
        {
            files.add(file);
			
		} 
        file = getNextFile();
    }
	closeDir();

	{
		wchar_t buf[128];
		wsprintf(buf, L"Developing %d images from: %s\n", files.size(), path.c_str());
		log(buf);
	}

	for (uint32_t i = 0; i < files.size() && !g_stopDevelop; i ++)
	{
		setProgress(2, i + 1, files.size());
		log(std::wstring(L"Developing: ") + files[i]);
		int res = on_Develop(path + files[i], dpath);

		if (res < 0)
        {
            err = true;
            break;
        }
        else
        {
            count += res;

			if (res == 0)
				Sleep(10);
        }
    }

    if (g_stopDevelop)
        log(L"==== Stopped by User ====");

    wchar_t buf[128];
    wsprintf(buf, L"==== %d Images Developed ====", count);

    if (!err)
        log(buf);
    else
        log(L"======== Error ========");

	setProgress(0, 0, 0);
    g_stopDevelop = false;
    g_goingDevelop = false;
    enableUI(true, 1);
}

void on_button_Tools_Go1()
{
    std::wstring file = browseFile(L"");

    if (file.length() == 0)
        return;

    clearLog();
    log(L"Processing Dark frame:");
    log(file);
    
    CR2_Hot(utf8_encode(file).c_str());
}

void on_button_Tools_Go2()
{
    std::wstring file = browseFile(L"");

    if (file.length() == 0)
        return;

    clearLog();
    log(L"Processing ADC offset frame:");
    log(file);

    CR2_Hot(utf8_encode(file).c_str());
}

void on_log(const char* text)
{
    log(utf8_decode(text));
}

//=====================================================================================
//
// Win32 crap
//
//=====================================================================================

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_RITDARKROOM, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_RITDARKROOM));

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_RITDARKROOM);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ICON1));

	return RegisterClassEx(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   //HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   InitCommonControls();

   DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), 0, About);

   PostQuitMessage(0);

   return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		{
			g_hDlg = hDlg;
//			redirectStdOut();
			on_startup();
			
			CoInitialize(NULL);
			HRESULT res = CoCreateInstance(
				CLSID_TaskbarList, NULL, CLSCTX_ALL,
				IID_ITaskbarList3, (void**)&g_pTaskBarlist);

			return (INT_PTR)TRUE;
		}
    case WM_MYMESSAGE:
      {
        PCOPYDATASTRUCT pMyCDS = (PCOPYDATASTRUCT) lParam;
        LPCSTR szString = (LPCSTR)(pMyCDS->lpData);
        log(utf8_decode(szString));
      }
      break;
	case WM_COMMAND:
		{
        int id = LOWORD(wParam);
        
        switch (id)
        {
        case IDC_IMPORT_B1:
            on_button_Import_Browse1();
            break;
        case IDC_IMPORT_B2:
            on_button_Import_Browse2();
            break;
        case IDC_IMPORT_B3:
            if (g_goingImport)
            {
                SetDlgItemText(g_hDlg, IDC_IMPORT_B3, L"STOPPING...");
                log(L"Stopping...");
                g_stopImport = true;
				setProgress(-1, 0, 0);
            }
            else
            {
                DWORD tid;
                HANDLE hid = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) &on_button_Import_Go, NULL, 0, &tid);
				SetThreadPriority(hid, THREAD_PRIORITY_BELOW_NORMAL);
            }
            break;
        case IDC_DEVELOP_B1:
            on_button_Develop_Browse1();
            break;
        case IDC_DEVELOP_B2:
            on_button_Develop_Browse2();
            break;
        case IDC_DEVELOP_B3:
            if (g_goingDevelop)
            {
                SetDlgItemText(g_hDlg, IDC_DEVELOP_B3, L"STOPPING...");
                log(L"Stopping...");
                g_stopDevelop = true;
				setProgress(-1, 0, 0);
            }
            else
            {
                DWORD tid;
                HANDLE hid = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) &on_button_Develop_Go, NULL, 0, &tid);
				SetThreadPriority(hid, THREAD_PRIORITY_BELOW_NORMAL);
            }
            break;
        case IDC_TOOLS_B1:
            on_button_Tools_Go1();
            break;
        case IDC_TOOLS_B2:
            on_button_Tools_Go2();
            break;
		}
		}
		break;
	case WM_CLOSE:
		{
			on_exit();
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
	}
	return (INT_PTR)FALSE;
}


void RedirectStdoutThreadRun()
{
    // Redirect stdout to pipe
    int fds[2];
    _pipe(fds, 16384, O_TEXT);
    _dup2(fds[1], 1); // 1 is stdout

    char buffer[16384];
    char buf2[16384];
    int nap = 500;

    for (;;)
    {
        Sleep(nap);
        printf(" ");

        // Need to flush the pipe
        fflush(stdout);

        // Read stdout from pipe
        DWORD dwNumberOfBytesRead = 0;

        dwNumberOfBytesRead = _read(fds[0], buffer, 16384 - 1);
        buffer[dwNumberOfBytesRead] = 0;

        if (dwNumberOfBytesRead > 1)
        {
            dwNumberOfBytesRead --;
            buffer[dwNumberOfBytesRead] = 0;

            // Send data as a message
            COPYDATASTRUCT myCDS;
            myCDS.dwData = MYPRINT;
            myCDS.cbData = dwNumberOfBytesRead + 1;

            char* bs = buffer, *bd = buf2;
            for (DWORD i = 0; i < dwNumberOfBytesRead; i ++)
            {
                if (*bs == '\n' && (i == dwNumberOfBytesRead - 1))
                    break;

                if (*bs == '\n')
                {
                    *bd ++ = '\r';
                    *bd ++ = *bs ++;
                }
                else
                {
                    *bd ++ = *bs ++;
                }
            }
            *bd = 0;

            myCDS.lpData = buf2;
            PostMessage(g_hDlg,
                        WM_MYMESSAGE,
                        0,
                        (LPARAM)(LPVOID) &myCDS);
            nap = 100;
        }
        else
        {
            nap = 500;
        }
    }
}

void redirectStdOut()
{
    // Create worker thread
    DWORD dwThreadId = 0;
    HANDLE m_hRedirectStdoutThread = CreateThread(
        // default security
        NULL,
        // default stack size
        0,
        // routine to execute
        (LPTHREAD_START_ROUTINE) &RedirectStdoutThreadRun,
        // thread parameter
        NULL,
        // immediately run the thread
        0,
        // thread Id
        &dwThreadId);

    if (NULL == m_hRedirectStdoutThread)
    {
        printf("Error creating stdin thread\n");
        return;
    }
}