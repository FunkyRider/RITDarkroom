#include "CR2Marshal.h"

CR2Marshal::CR2Marshal(SOCKET sock)
    : m_sock(sock)
{
    m_header = (unsigned char*) malloc(4096);
}

CR2Marshal::~CR2Marshal()
{
    free(m_header);
}

int CR2Marshal::open()
{
    int res;
    char bufTempName[MAX_PATH] = {0};
    {
        char bufPath[MAX_PATH];
        GetTempPath(MAX_PATH, bufPath);
        GetTempFileName(bufPath, "_CR2", 0, bufTempName);
    }

    unsigned int fsize = 0;
    res = recv(m_sock, (char*) &fsize, sizeof (unsigned int), 0);

    printf("CR2 File size: %d\n", fsize);

    if (res == 0)
        return 0;

    FILE* fp = NULL;

    fopen_s(&fp, bufTempName, "wb+");    

    if (!fp)
        return 0;

    m_fileName = bufTempName;

    unsigned int frecv = 0;

    char* dbuf = (char*) malloc(16384);
    bool isHeader = true;

    while (frecv < fsize)
    {
        res = recv(m_sock, dbuf, 16384, 0);

        if (isHeader)
        {
            memcpy(m_header, dbuf, 4096);
            isHeader = false;
        }

        if (res == 0)
        {
            fclose(fp);
            DeleteFile(m_fileName.c_str());
            m_fileName.clear();
            return 0;
        }
        frecv += res;

        fwrite(dbuf, 1, res, fp);
    }
    
    free(dbuf);
    fclose(fp);

    if (m_cr2.open(m_fileName) != 0)
    {
        DeleteFile(m_fileName.c_str());
        m_fileName.clear();
        return 0;
    }

    const char ack = 1;
    send(m_sock, &ack, sizeof (char), 0);

    unsigned int dimension[2] = {m_cr2.width(), m_cr2.height()};
    send(m_sock, (char*) dimension, sizeof (unsigned int) * 2, 0);

    return 1;
}

int CR2Marshal::save()
{
    if (m_fileName.length() == 0)
        return 0;

    m_cr2.save();
    m_cr2.close();

    // Find VRD..
    unsigned int vrdpos, vrdlen;
    CR2Info::ReadVRD(m_fileName.c_str(), vrdpos, vrdlen);

    if (vrdlen == 0)
        return 0;

    FILE* fp = NULL;
    void* buf = malloc(vrdlen);

    fopen_s(&fp, m_fileName.c_str(), "rb");
    unsigned int vrdppos = 0, vrdpval = 0;

    // Find VRD position d-word
    for (int i = 0; i < 4096; i ++)
    {
        unsigned char c = fgetc(fp);

        if (c != m_header[i])
        {
            vrdppos = i;
            vrdpval = *(reinterpret_cast<unsigned int*>(m_header + i));
        }
    }

    // Read VRD block
    if (vrdppos > 0)
    {
        fseek(fp, 0, SEEK_SET);
        fread(m_header, 1, 4096, fp);
    }

    fseek(fp, vrdpos, SEEK_SET);
    fread(buf, 1, vrdlen, fp);
    fclose(fp);

    m_cr2.open(m_fileName);

    const char ack = 1;
    send(m_sock, &ack, sizeof (char), 0);

    if (vrdppos > 0)
    {
        bulkSend(m_header, 4096);
    }
    else
    {
        bulkSend(m_header, 0);
    }

    send(m_sock, (char*) &vrdpos, sizeof (unsigned int), 0);

    unsigned int sent = bulkSend(buf, vrdlen);
    
    free(buf);

    if (sent < vrdlen)
        return 0;

    return 1;
}

int CR2Marshal::close()
{
    if (m_fileName.length() == 0)
        return 0;

    m_cr2.close();
    //DeleteFile(m_fileName.c_str());

    m_fileName.clear();

    const char ack = 1;
    send(m_sock, &ack, sizeof (char), 0);
    return 1;
}

int CR2Marshal::getExif()
{
    if (m_fileName.length() == 0)
        return 0;

    const char ack = 1;
    string data = m_cr2.getExif();
    unsigned int len = data.length() + 1; 

    int i = sizeof (unsigned int);

    send(m_sock, &ack, sizeof (char), 0);
    send(m_sock, (char*) &len, sizeof (unsigned int), 0);
    send(m_sock, data.c_str(), len, 0);

    return 1;
}

int CR2Marshal::getAdjustments()
{
    if (m_fileName.length() == 0)
        return 0;

    const char ack = 1;
    string data = m_cr2.getAdjustments();
    unsigned int len = data.length() + 1; 

    send(m_sock, &ack, sizeof (char), 0);
    send(m_sock, (char*) &len, sizeof (unsigned int), 0);
    send(m_sock, data.c_str(), len, 0);

    return 1;
}

int CR2Marshal::setAdjustment()
{
   string arg, val;
   char buf[256];
   unsigned int len;
   const char ack = 1;

   recv(m_sock, (char*) &len, sizeof (unsigned int), 0);
   if (len > 255) len = 255;
   recv(m_sock, buf, len, 0);
   buf[len] = 0;
   arg = buf;

   recv(m_sock, (char*) &len, sizeof (unsigned int), 0);
   if (len > 255) len = 255;
   recv(m_sock, buf, len, 0);
   buf[len] = 0;
   val = buf;

   if (m_fileName.length() == 0)
        return 0;

   m_cr2.setAdjustment(arg, val);

   send(m_sock, &ack, sizeof (char), 0);
   return 1;
}

int CR2Marshal::resetToCapture()
{
    if (m_fileName.length() == 0)
        return 0;

    m_cr2.resetToCapture();

    char ack = 1;
    send(m_sock, &ack, sizeof (char), 0);
    return 1;
}

int CR2Marshal::getFull()
{
    if (m_fileName.length() == 0)
        return 0;

    char ack = 1;
    send(m_sock, &ack, sizeof (char), 0);

    int w = m_cr2.width(), h = m_cr2.height();
    unsigned int len = w * h * 3;
    void* buf = malloc(len);
    m_cr2.getFull(buf);

    unsigned int sent = bulkSend(buf, len);
    
    free(buf);

    if (sent < len)
        return 0;

    return 1;
}

int CR2Marshal::getScaled()
{
    unsigned int size[2];
    recv(m_sock, (char*) size, sizeof (unsigned int) * 2, 0);

    if (m_fileName.length() == 0)
        return 0;

    char ack = 1;
    send(m_sock, &ack, sizeof (char), 0);

    int w = size[0], h = size[1];
    unsigned int len = w * h * 3;
    void* buf = malloc(len);
    m_cr2.getScaled(buf, w, h);

    unsigned int sent = bulkSend(buf, len);
    
    free(buf);

    if (sent < len)
        return 0;

    return 1;
}

int CR2Marshal::getRegion()
{
    unsigned int size[4];
    recv(m_sock, (char*) size, sizeof (unsigned int) * 4, 0);

    if (m_fileName.length() == 0)
        return 0;

    char ack = 1;
    send(m_sock, &ack, sizeof (char), 0);

    int w = size[2], h = size[3];
    unsigned int len = w * h * 3;
    void* buf = malloc(len);
    m_cr2.getRegion(buf, size[0], size[1], w, h);

    unsigned int sent = bulkSend(buf, len);
    
    free(buf);

    if (sent < len)
        return 0;

    return 1;
}

unsigned int CR2Marshal::bulkSend(void* buf, unsigned int len)
{
    send(m_sock, (char*) &len, sizeof (unsigned int), 0);

    unsigned int fsent = 0;
    char* ptr = (char*) buf;

    while (fsent < len)
    {
        unsigned int sent;
        unsigned int left = len - fsent;

        if (left > 16384)
            left = 16384;

        sent = send(m_sock, ptr, left, 0);
        if (sent == 0)
        {
            return fsent;
        }
        
        ptr += sent;
        fsent += sent;
    }

    return fsent;
}
