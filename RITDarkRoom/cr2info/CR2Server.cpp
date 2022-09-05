#include "CR2Marshal.h"
#include "CR2Server.h"

#pragma comment (lib, "Ws2_32.lib")

CR2Server::CR2Server(int port)
{
    m_data = (_CR2ThreadData*) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(_CR2ThreadData) * CR2S_MAXTHREAD);
    WSADATA wsa;

    WSAStartup(MAKEWORD(2,2), &wsa);

    m_port = port;

    for (int i = 0; i < CR2S_MAXTHREAD; i ++)
    {
        m_thread[i] = 0;
        m_id[i] = 0;
        m_socket[i] = INVALID_SOCKET;

        m_data[i].id = i;
        m_data[i].obj = this;
    }

    m_thread[0] = CreateThread(NULL, 0, ListenThread, this, 0, &m_id[0]);
}

CR2Server::~CR2Server()
{
    shutdown(m_socket[0], SD_BOTH);

    SOCKET s = m_socket[0];
    m_socket[0] = INVALID_SOCKET;

    closesocket(s);
    wait();

    WSACleanup();

    HeapFree(GetProcessHeap(), 0, m_data);
}

void CR2Server::wait()
{
    WaitForSingleObject(m_thread[0], INFINITE);
}

int CR2Server::connectionCount() const
{
    int c = 0;

    for (int i = 1; i < CR2S_MAXTHREAD; i ++)
    {
        if (m_thread[i] != 0)
            c ++;
    }

    return c;
}

int CR2Server::getEmptyID() const
{
    for (int i = 1; i < CR2S_MAXTHREAD; i ++)
    {
        if (m_thread[i] == 0)
            return i;
    }

    return 0;
}

DWORD WINAPI CR2Server::ListenThread(LPVOID param)
{
    CR2Server* that = (CR2Server*) param;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    {
        char pbuf[16];
        _itoa_s(that->m_port, pbuf, 10);
        getaddrinfo(NULL, pbuf, &hints, &result);
    }

    that->m_socket[0] = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    bind(that->m_socket[0], result->ai_addr, (int)result->ai_addrlen);

    freeaddrinfo(result);

    printf("Server listening started, port %d...\n", that->m_port);

    while (that->m_socket[0] != INVALID_SOCKET)
    {
        listen(that->m_socket[0], CR2S_MAXTHREAD);
        SOCKET tmpSock = accept(that->m_socket[0], NULL, NULL);
        
        int id = that->getEmptyID();

        if (id == 0)
        {
            closesocket(tmpSock);
            continue;
        }

        that->m_socket[id] = tmpSock;
        that->m_thread[id] = CreateThread(NULL, 0, ServerThread, &that->m_data[id], 0, &that->m_id[id]);
    }

    return 0;
}

DWORD WINAPI CR2Server::ServerThread(LPVOID param)
{
    _CR2ThreadData* data = (_CR2ThreadData*) param;
    SOCKET sock = data->obj->m_socket[data->id];
    CR2Marshal cr2(sock);

    int res = 0;
    char command = 0;

    printf("Server connected, ID: %d\n", data->id);

    do {
        res = recv(sock, &command, sizeof(char), 0);

        if (res > 0)
        {
            int ret = 0;

            printf("Command[%d]: %d\n", data->id, command);

            switch (command)
            {
            case 0:         // disconnect
                res = 0;
                break;
            case 1:         // open, get cr2 data, create temp file
                ret = cr2.open();
                break;
            case 2:         // save, send cr2 VRD block
                ret = cr2.save();
                break;
            case 3:         // close, del cr2 temp file
                ret = cr2.close();
                break;
            case 4:         // getExif
                ret = cr2.getExif();
                break;
            case 5:         // getAdjustments
                ret = cr2.getAdjustments();
                break;
            case 6:         // setAdjustment
                ret = cr2.setAdjustment();
                break;
            case 7:         // resetToCapture
                ret = cr2.resetToCapture();
                break;
            case 8:         // getFull, send RGB data
                ret = cr2.getFull();
                break;
            case 9:         // getScaled, send RGB data
                ret = cr2.getScaled();
                break;
            case 10:        // getRegion, send RGB data
                ret = cr2.getRegion();
                break;
            default:        // error, unknown command
                break;
            }

            if (ret == 0)
            {
                res = 0;
                break;
            }
        }
    } while (res > 0);

    shutdown(sock, SD_BOTH);
    closesocket(sock);

    data->obj->m_id[data->id] = 0;
    data->obj->m_thread[data->id] = 0;

    printf("Server disconnected, ID: %d\n", data->id);

    ExitThread(0);

    return 0;
}