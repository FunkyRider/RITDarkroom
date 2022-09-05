#include "cr2client.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h> 

CR2Client::CR2Client(const char* ip, int port)
{
	struct sockaddr_in serv_addr;
    struct hostent *server;
    
    m_sock = socket(AF_INET, SOCK_STREAM, 0);
    
    server = gethostbyname(ip);
    if (server == NULL)
    {
    	printf("Error find server\n");
    	::close(m_sock);
    	return;
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(port);
    
    if (connect(m_sock,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
    {
    	printf("Error connect to server\n");
    	::close(m_sock);
    	return;
    }
}

CR2Client::~CR2Client()
{
	::close(m_sock);
}

int CR2Client::open(string path)
{
	char cmd = 1;
	send(m_sock, &cmd, sizeof (char), 0);
	
	struct stat st;
	if (stat(path.c_str(), &st) != 0)
		return -1;
	
	uint32_t size = st.st_size;
	
	send(m_sock, (char*) &size, sizeof (uint32_t), 0);
	
	FILE *fp = fopen(path.c_str(), "rb");
	
	if (fp == NULL)
		return -1;
	
	void* buf = malloc(16384);
	size_t len;
	
	for (;;)
	{
		len = fread(buf, 1, 16384, fp);

		if (len <= 0)
			break;
		
		size_t sent = send(m_sock, (char*) buf, len, 0);
		
		//printf("Sending bytes: %u          \r", (uint32_t) sent);
		if (sent < len)
		{
			printf("CR2 file send error\n");
			break;
		}
	}
	
	free(buf);
	
	fclose(fp);
	recv(m_sock, &cmd, sizeof (char), 0);

	if (cmd == 1)
	{
		m_fileName = path;
		
		uint32_t dim[2];
		recv(m_sock, (char*) dim, sizeof (uint32_t) * 2, 0);
		_w = dim[0];
		_h = dim[1];
	}
	else
	{
		m_fileName.clear();
		_w = _h = 0;
	}
	
	return cmd;
}

int CR2Client::save()
{
	const char* VRD_SIGNATURE = "CANON OPTIONAL DATA";
	
	char cmd = 2;
	send(m_sock, &cmd, sizeof (char), 0);
	
	recv(m_sock, &cmd, sizeof (char), 0);
	if (cmd != 1)
	{
		printf("Error getting CR2 VRD\n");
		return 0;
	}

	uint32_t hlen;
	recv(m_sock, (char*) &hlen, sizeof (uint32_t), 0);	
	void* header = hlen > 0 ? malloc(hlen) : NULL;
	if (hlen > 0)
	{
		bulkRecv(header, hlen);
	}
	
	uint32_t pos, len;
	recv(m_sock, (char*) &pos, sizeof (uint32_t), 0);
	recv(m_sock, (char*) &len, sizeof (uint32_t), 0);
	
	void* buf = malloc(len);
	bulkRecv(buf, len);

	FILE *fp = fopen(m_fileName.c_str(), "rb+");
	
	if (fp == NULL)
	{
		printf("Error saving local CR2\n");
		free(buf);
		return 0;
	}
	
	if (hlen > 0)
	{
		// Add new VRD section
		printf("writing header of size %d\n", hlen);
		fseek(fp, 0, SEEK_SET);
		fwrite(header, 1, hlen, fp);
	}
	else
	{
		fseek(fp, pos, SEEK_SET);
		printf("Reading VRD at %08X\n", pos);
		// Update existing VRD section
		// Verify that we are writing to the correct position!
		char header[0x1c];
		if (fread(header, 1, 0x1c, fp) != 0x1c)
		{
			printf("Error CR2 VRD block read error, saving aborted\n");
			return 0;
		}
	
		if (strncmp(VRD_SIGNATURE, header, 19) != 0)
		{
		    fclose(fp);
		    free(buf);
		    printf("Error CR2 VRD block not found, saving aborted\n");
		    return 0;
		}		
	}
	
	fseek(fp, pos, SEEK_SET);
	
    uint32_t wlen = fwrite(buf, 1, len, fp);
    
    if (wlen < len)
    {
    	printf("Error updating CR2 VRD block, bytes written: %u\n", wlen);
    	fclose(fp);
    	free(buf);
    	return 0;
    }
    
    printf("CR2 VRD block updated at: 0x%08X : 0x%08X\n", pos, len);
	
	fclose(fp);
	free(buf);
	
	return cmd;
}

void CR2Client::close()
{
	char cmd = 3;
	send(m_sock, &cmd, sizeof (char), 0);
	recv(m_sock, &cmd, sizeof (char), 0);
	
	if (cmd != 1)
		printf("CR2 file close error\n");
	
	m_fileName.clear();
	
	return;
}

string CR2Client::getExif()
{
	char cmd = 4;
	send(m_sock, &cmd, sizeof (char), 0);
	
	recv(m_sock, &cmd, sizeof (char), 0);
	if (cmd != 1)
	{
		printf("Error getting Exif\n");
		return "";
	}
	
	uint32_t len = 0;
	recv(m_sock, (char*) &len, sizeof(uint32_t), 0);
	
	char* buf = (char*) malloc(len + 1);
	recv(m_sock, buf, len, 0);
	buf[len] = 0;
	
	string ret = buf;
	free(buf);

	return ret;
}

string CR2Client::getAdjustments()
{
	char cmd = 5;
	send(m_sock, &cmd, sizeof (char), 0);
	
	recv(m_sock, &cmd, sizeof (char), 0);
	if (cmd != 1)
	{
		printf("Error getting Adjustments\n");
		return "";
	}
		
	uint32_t len = 0;
	recv(m_sock, (char*) &len, sizeof(uint32_t), 0);
	
	char* buf = (char*) malloc(len + 1);
	recv(m_sock, buf, len, 0);
	buf[len] = 0;
	
	string ret = buf;
	free(buf);

	return ret;
}

int CR2Client::setAdjustment(string param, string value)
{
	char cmd = 6;
	send(m_sock, &cmd, sizeof (char), 0);
	
	uint32_t len;
	len = param.length();
	send(m_sock, (char*) &len, sizeof (uint32_t), 0);
	send(m_sock, param.c_str(), len, 0);
	
	len = value.length();
	send(m_sock, (char*) &len, sizeof (uint32_t), 0);
	send(m_sock, value.c_str(), len, 0);

	recv(m_sock, &cmd, sizeof (char), 0);
	if (cmd != 1)
	{
		printf("Error setting Adjustment\n");
		return 0;
	}
	
	return cmd;
}

int CR2Client::resetToCapture()
{
	char cmd = 7;
	send(m_sock, &cmd, sizeof (char), 0);
	
	recv(m_sock, &cmd, sizeof (char), 0);
	if (cmd != 1)
	{
		printf("Error resetting Adjustments\n");
		return 0;
	}
	
	return cmd;
}

int CR2Client::width()
{
    return _w;
}

int CR2Client::height()
{
    return _h;
}

int CR2Client::getFull(void* ptr)
{
	char cmd = 8;
	send(m_sock, &cmd, sizeof (char), 0);

	recv(m_sock, &cmd, sizeof (char), 0);
	if (cmd != 1)
	{
		printf("Error getting RGB data\n");
		return 0;
	}
	
	int res = bulkRecv(ptr);
	
	return (res > 0) ? cmd : 0;
}

int CR2Client::getScaled(void* ptr, int dw, int dh)
{
	char cmd = 9;
	send(m_sock, &cmd, sizeof (char), 0);
	
	uint32_t dimension[2] = {dw, dh};
	send(m_sock, (char*) dimension, sizeof (uint32_t) * 2, 0);

	recv(m_sock, &cmd, sizeof (char), 0);
	if (cmd != 1)
	{
		printf("Error getting RGB data\n");
		return 0;
	}
	
	int res = bulkRecv(ptr);
	
	return (res > 0) ? cmd : 0;
}

int CR2Client::getRegion(void* ptr, int x, int y, int w, int h)
{
	char cmd = 10;
	send(m_sock, &cmd, sizeof (char), 0);
	
	uint32_t dimension[4] = {x, y, w, h};
	send(m_sock, (char*) dimension, sizeof (uint32_t) * 4, 0);

	recv(m_sock, &cmd, sizeof (char), 0);
	if (cmd != 1)
	{
		printf("Error getting RGB data\n");
		return 0;
	}
	
	int res = bulkRecv(ptr);
	
	return (res > 0) ? cmd : 0;
}

int CR2Client::bulkRecv(void* ptr, uint32_t size)
{
	if (size == 0)
	{
		recv(m_sock, (char*) &size, sizeof (uint32_t), 0);
	}
	
	char* buf = (char*) ptr;
	uint32_t len, lrecv = 0;
	
	while (lrecv < size)
	{
		len = recv(m_sock, buf, 16384, 0);
		
		if (len == 0)
			break;
		
		buf += len;
		lrecv += len;
	}
	
	if (lrecv != size)
	{
		printf("Error getting RGB data\n");
		return 0; 
	}
	
	return lrecv;
}
