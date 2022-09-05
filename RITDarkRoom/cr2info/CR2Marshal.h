#pragma once

#include "CR2Info.h"

class CR2Marshal
{
public:
    CR2Marshal(SOCKET sock);
    ~CR2Marshal();

    int     open();
    int     save();
    int     close();
    int     getExif();
    int     getAdjustments();
    int     setAdjustment();
    int     resetToCapture();
    int     getFull();
    int     getScaled();
    int     getRegion();

private:
    CR2Info     m_cr2;
    SOCKET      m_sock;
    string      m_fileName;
    unsigned char*  m_header;

    unsigned int    bulkSend(void* buf, unsigned int len);
};