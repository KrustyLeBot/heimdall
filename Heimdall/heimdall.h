#include <stdio.h>
#include <winsock2.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <ctime>

#include "datastructures.h"

struct Client
{
    struct sockaddr_in si_other;
    ClientInfo info;
};

class Heimdall
{
public:
    ////////////////////////////////////////
    //Server
    void InitServer(int port);
    void UnInitServer();
    void CallbackServerListen(Client client);
    void UpdateServer();
    ////////////////////////////////////////

    ////////////////////////////////////////
    //Client
    void InitClient(std::string serverAddr, int serverPort);
    void UnInitClient();
    void CallbackClientListen(const std::vector<ClientInfo>& clientInfos);
    void SendInfoToServer(const ClientInfo info);
    ////////////////////////////////////////

    ////////////////////////////////////////
    //Common
    unsigned int GetClientHash(const Client& client);
    ////////////////////////////////////////

public:
    ////////////////////////////////////////
    //Common
    bool alreadyInit = false;
    SOCKET m_socket;
    struct sockaddr_in m_sockaddr;
    int m_slen = sizeof(sockaddr_in);
    WSADATA m_wsa;

    DWORD m_rcvThreadId;
    bool m_rcvThreadCreated = false;
    HANDLE m_rcvThreadHandle;

    DWORD m_sendThreadId;
    bool m_sendThreadCreated = false;
    HANDLE m_sendThreadHandle;
    ////////////////////////////////////////

    ////////////////////////////////////////
    //Server
    HANDLE m_serverInfoMutex;
    std::unordered_map<unsigned int, Client> m_clients;
    std::unordered_map<unsigned int, std::time_t> m_clientsLastPacketTime;
    std::vector<std::vector<ClientInfo>> m_packetsToSend;
    ////////////////////////////////////////

    ////////////////////////////////////////
    //Client
    HANDLE m_clientInfoMutex;
    ClientInfo m_info;
    bool m_infoNeedToBeSent = false;
    std::vector<ClientInfo> m_clientInfos;
    ////////////////////////////////////////
};

DWORD WINAPI ServerListenTask(LPVOID lpParam);
DWORD WINAPI SendToClientsTask(LPVOID lpParam);

DWORD WINAPI ClientListenTask(LPVOID lpParam);
DWORD WINAPI SendToServerTask(LPVOID lpParam);