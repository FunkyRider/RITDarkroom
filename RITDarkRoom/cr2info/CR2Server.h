#pragma once

#define CR2S_MAXTHREAD  16

class CR2Server;

struct _CR2ThreadData
{
    CR2Server*  obj;
    int         id;
};

class CR2Server
{
public:
    CR2Server(int port);
    ~CR2Server();
    void    wait();
    int     connectionCount() const;

private:
    int     getEmptyID() const;

    static DWORD WINAPI ListenThread(LPVOID param);
    static DWORD WINAPI ServerThread(LPVOID param);

    int     m_port;
    HANDLE  m_thread[CR2S_MAXTHREAD];
    DWORD   m_id[CR2S_MAXTHREAD];
    SOCKET  m_socket[CR2S_MAXTHREAD];

    _CR2ThreadData  *m_data;
};