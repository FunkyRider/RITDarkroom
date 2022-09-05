#include <stdio.h>
#include <stdint.h>
#include <string>

using namespace std;

class CR2Client
{
public:
	CR2Client(const char* ip, int port);
	~CR2Client();
	
	int     open(string path);
    int     save();
    void    close();

	int     width();
    int     height();
    string  getExif();
    string  getAdjustments();
    int     setAdjustment(string param, string value);
    int     resetToCapture();

    int     getFull(void* ptr);
    int     getScaled(void* ptr, int dw, int dh);
    int     getRegion(void* ptr, int x, int y, int w, int h);
	
private:
	int		bulkRecv(void* ptr, uint32_t size = 0);
	
	int		m_sock;
	string	m_fileName;
	int		_w, _h;
};
