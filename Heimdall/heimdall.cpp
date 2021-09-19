#include "heimdall.h"
#include <stdio.h>
#include <iostream>

#define MAX_DATA_STRUCTUTRES_PER_PACKET 10
#define CLIENT_TIMEOUT_SECOND 10

//////////////////////////////////////////////////////////////////////////////////////////////////
#pragma comment(lib, "ws2_32.lib")
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
//////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////
// Heimdall Server

void Heimdall::InitServer(int port)
{
	if (alreadyInit)
	{
		std::cout << "A server or client was already created and init for this Heimdall object" << std::endl;
		return;
	}

	alreadyInit = true;

	//Initialise winsock
	std::cout << "Initialising Winsock..." << std::endl;
	if (WSAStartup(MAKEWORD(2, 2), &m_wsa) != 0)
	{
		std::cout << "Failed. Error Code :" << WSAGetLastError() << std::endl;
		exit(EXIT_FAILURE);
	}
	std::cout << "Initialised." << std::endl;

	//Create a socket
	if ((m_socket = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
		std::cout << "Could not create socket : " << WSAGetLastError() << std::endl;
	}
	std::cout << "Socket created." << std::endl;

	BOOL bNewBehavior = FALSE;
	DWORD dwBytesReturned = 0;
	WSAIoctl(m_socket, SIO_UDP_CONNRESET, &bNewBehavior, sizeof bNewBehavior, NULL, 0, &dwBytesReturned, NULL, NULL);

	//Prepare the sockaddr_in structure
	m_sockaddr.sin_family = AF_INET;
	m_sockaddr.sin_addr.s_addr = INADDR_ANY;
	m_sockaddr.sin_port = htons(port);

	//Bind
	if (bind(m_socket, (struct sockaddr *)&m_sockaddr, sizeof(m_sockaddr)) == SOCKET_ERROR)
	{
		std::cout << "Bind failed with error code : " << WSAGetLastError() << std::endl;
		exit(EXIT_FAILURE);
	}
	std::cout << "Bind done" << std::endl;

	m_serverInfoMutex = CreateMutex(
		NULL,  // default security attributes
		FALSE, // initially not owned
		NULL); // unnamed mutex

	if (m_clientInfoMutex == NULL)
	{
		std::cout << "CreateMutex error: " << GetLastError() << std::endl;
		exit(EXIT_FAILURE);
	}

	m_rcvThreadHandle = CreateThread(
		NULL,			  // default security attributes
		0,				  // use default stack size
		ServerListenTask, // thread function name
		this,			  // argument to thread function
		0,				  // use default creation flags
		&m_rcvThreadId);  // returns the thread identifier

	if (m_rcvThreadHandle == NULL)
	{
		std::cout << "CreateThread error: " << GetLastError() << std::endl;
		exit(EXIT_FAILURE);
	}
	m_rcvThreadCreated = true;

	m_sendThreadHandle = CreateThread(
		NULL,			   // default security attributes
		0,				   // use default stack size
		SendToClientsTask, // thread function name
		this,			   // argument to thread function
		0,				   // use default creation flags
		&m_sendThreadId);  // returns the thread identifier

	if (m_sendThreadHandle == NULL)
	{
		std::cout << "CreateThread error: " << GetLastError() << std::endl;
		exit(EXIT_FAILURE);
	}
	m_sendThreadCreated = true;
}

void Heimdall::UnInitServer()
{
	CloseHandle(m_serverInfoMutex);

	TerminateThread(
		m_rcvThreadHandle,
		0);

	TerminateThread(
		m_sendThreadHandle,
		0);

	closesocket(m_socket);
	WSACleanup();
}

DWORD WINAPI ServerListenTask(LPVOID lpParam)
{
	Heimdall *heimdall = reinterpret_cast<Heimdall *>(lpParam);

	while (true)
	{
		Client client;

		//try to receive some data, this is a blocking call
		if ((recvfrom(heimdall->m_socket, (char *)&client.info, sizeof(ClientInfo), 0, (struct sockaddr *)&client.si_other, &heimdall->m_slen)) == SOCKET_ERROR)
		{
			std::cout << "recvfrom() failed with error code : " << WSAGetLastError() << std::endl;
			continue;
		}

		heimdall->CallbackServerListen(client);
	}

	return 0;
}

DWORD WINAPI SendToClientsTask(LPVOID lpParam)
{
	Heimdall *heimdall = reinterpret_cast<Heimdall *>(lpParam);

	while (true)
	{
		WaitForSingleObject(
			heimdall->m_serverInfoMutex, // handle to mutex
			INFINITE);

		//foreach client, send them clientinfo of everyone
		for (const auto &packet : heimdall->m_packetsToSend)
		{
			//create a buffer with 8 bits for the number of client info + the list of client info
			size_t bufSize = 8 + sizeof(ClientInfo) * packet.size();
			char buf[bufSize];
			buf[0] = packet.size();
			memcpy(&buf[1], (char*)packet.data(), sizeof(ClientInfo) * packet.size());

			for (const auto &client : heimdall->m_clients)
			{
				if (sendto(heimdall->m_socket, (char *)buf, bufSize, 0, (struct sockaddr *)&client.second.si_other, heimdall->m_slen) == SOCKET_ERROR)
				{
					std::cout << "sendto() failed with error code : " << WSAGetLastError() << std::endl;
				}
			}
		}

		heimdall->m_packetsToSend.clear();

		ReleaseMutex(heimdall->m_serverInfoMutex);
	}
	return 0;
}

void Heimdall::UpdateServer()
{
	WaitForSingleObject(
		m_serverInfoMutex, // handle to mutex
		INFINITE);

	if (m_clients.size() == 0)
	{
		ReleaseMutex(m_serverInfoMutex);
		return;
	}

	//remove clients that no longer send packets
	std::vector<unsigned int> clientsToRemove;
	std::time_t now = std::time(0);
	for (const auto &client : m_clientsLastPacketTime)
	{
		if(std::difftime(now, client.second) > CLIENT_TIMEOUT_SECOND)
		{
			clientsToRemove.push_back(client.first);
		}
	}

	for (const auto &clientHash : clientsToRemove)
	{
		m_clients.erase(clientHash);
		m_clientsLastPacketTime.erase(clientHash);
	}

	//generate new packets to send to clients
	std::vector<ClientInfo> packet;
	m_packetsToSend.push_back(packet);

	//construct packets to send
	for (const auto &client : m_clients)
	{
		if (m_packetsToSend[-1].size() == MAX_DATA_STRUCTUTRES_PER_PACKET)
		{
			std::vector<ClientInfo> newPacket;
			m_packetsToSend.push_back(newPacket);
		}

		m_packetsToSend.back().push_back(client.second.info);
	}

	ReleaseMutex(m_serverInfoMutex);
}

void Heimdall::CallbackServerListen(Client client)
{
	std::cout << "Received packet from " << inet_ntoa(client.si_other.sin_addr) << ": " << ntohs(client.si_other.sin_port) << std::endl;

	WaitForSingleObject(
		m_serverInfoMutex, // handle to mutex
		INFINITE);

	std::cout << GetClientHash(client) << std::endl;

	m_clients[GetClientHash(client)] = client;
	m_clientsLastPacketTime[GetClientHash(client)] = std::time(0);

	for (const auto &client : m_clients)
	{
		std::cout << "Client: " << client.first << std::endl;
		std::cout << "	color: " << client.second.info.color << std::endl;
		std::cout << "	x: " << client.second.info.x << std::endl;
		std::cout << "	y: " << client.second.info.y << std::endl;
		std::cout << "	z: " << client.second.info.z << std::endl;
	}

	ReleaseMutex(m_serverInfoMutex);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////
// Heimdall Client

void Heimdall::InitClient(std::string serverAddr, int serverPort)
{
	if (alreadyInit)
	{
		std::cout << "A server or client was already created and init for this Heimdall object" << std::endl;
		return;
	}

	alreadyInit = true;

	//Initialise winsock
	std::cout << "Initialising Winsock..." << std::endl;
	if (WSAStartup(MAKEWORD(2, 2), &m_wsa) != 0)
	{
		std::cout << "Failed. Error Code : " << WSAGetLastError() << std::endl;
		exit(EXIT_FAILURE);
	}
	std::cout << "Initialised." << std::endl;

	//create socket
	if ((m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
	{
		std::cout << "socket() failed with error code : " << WSAGetLastError() << std::endl;
		exit(EXIT_FAILURE);
	}

	BOOL bNewBehavior = FALSE;
	DWORD dwBytesReturned = 0;
	WSAIoctl(m_socket, SIO_UDP_CONNRESET, &bNewBehavior, sizeof bNewBehavior, NULL, 0, &dwBytesReturned, NULL, NULL);

	//setup address structure
	m_sockaddr.sin_family = AF_INET;
	m_sockaddr.sin_port = htons(serverPort);
	m_sockaddr.sin_addr.S_un.S_addr = inet_addr(serverAddr.c_str());

	m_clientInfoMutex = CreateMutex(
		NULL,  // default security attributes
		FALSE, // initially not owned
		NULL); // unnamed mutex

	if (m_clientInfoMutex == NULL)
	{
		std::cout << "CreateMutex error: " << GetLastError() << std::endl;
		exit(EXIT_FAILURE);
	}

	m_sendThreadHandle = CreateThread(
		NULL,			  // default security attributes
		0,				  // use default stack size
		SendToServerTask, // thread function name
		this,			  // argument to thread function
		0,				  // use default creation flags
		&m_sendThreadId); // returns the thread identifier

	if (m_sendThreadHandle == NULL)
	{
		std::cout << "CreateThread error: " << GetLastError() << std::endl;
		exit(EXIT_FAILURE);
	}
	m_sendThreadCreated = true;
}

void Heimdall::UnInitClient()
{
	CloseHandle(m_clientInfoMutex);

	TerminateThread(
		m_rcvThreadHandle,
		0);

	TerminateThread(
		m_sendThreadHandle,
		0);

	closesocket(m_socket);
	WSACleanup();
}

DWORD WINAPI ClientListenTask(LPVOID lpParam)
{
	Heimdall *heimdall = reinterpret_cast<Heimdall *>(lpParam);

	while (true)
	{
		//create a buffer with 8 bits for the number of client info + the list of client info
		size_t bufSize = 8 + (MAX_DATA_STRUCTUTRES_PER_PACKET * sizeof(ClientInfo));
		char buf[bufSize];
		struct sockaddr_in si_other;

		//try to receive some data, this is a blocking call
		if (recvfrom(heimdall->m_socket, buf, bufSize, 0, (struct sockaddr *)&si_other, &heimdall->m_slen) == SOCKET_ERROR)
		{
			std::cout << "recvfrom() failed with error code : " << WSAGetLastError() << std::endl;
			continue;
		}

		int nbClientInfoOnPacket = (int)(buf[0]);
		ClientInfo *clientList = (ClientInfo *)(&buf[1]);
		std::vector<ClientInfo> clientInfos;
		clientInfos.assign(clientList, clientList + nbClientInfoOnPacket);

		heimdall->CallbackClientListen(clientInfos);
	}

	return 0;
}

DWORD WINAPI SendToServerTask(LPVOID lpParam)
{
	Heimdall *heimdall = reinterpret_cast<Heimdall *>(lpParam);

	while (true)
	{
		WaitForSingleObject(
			heimdall->m_clientInfoMutex, // handle to mutex
			INFINITE);					 // no time-out interval

		if (!heimdall->m_infoNeedToBeSent)
		{
			ReleaseMutex(heimdall->m_clientInfoMutex);
			continue;
		}

		if (sendto(heimdall->m_socket, (char *)&heimdall->m_info, sizeof(ClientInfo), 0, (struct sockaddr *)&heimdall->m_sockaddr, heimdall->m_slen) == SOCKET_ERROR)
		{
			printf("sendto() failed with error code : %d", WSAGetLastError());
		}

		heimdall->m_infoNeedToBeSent = false;

		//we created the receiving thread when we send a packet to server for the 1st time
		if (!heimdall->m_rcvThreadCreated)
		{
			heimdall->m_rcvThreadHandle = CreateThread(
				NULL,					   // default security attributes
				0,						   // use default stack size
				ClientListenTask,		   // thread function name
				heimdall,				   // argument to thread function
				0,						   // use default creation flags
				&heimdall->m_rcvThreadId); // returns the thread identifier

			if (heimdall->m_rcvThreadHandle == NULL)
			{
				std::cout << "CreateThread error: " << GetLastError() << std::endl;
				exit(EXIT_FAILURE);
			}
			heimdall->m_rcvThreadCreated = true;
		}

		ReleaseMutex(heimdall->m_clientInfoMutex);
	}

	return 0;
}

void Heimdall::CallbackClientListen(const std::vector<ClientInfo>& clientInfos)
{
	WaitForSingleObject(
		m_clientInfoMutex, // handle to mutex
		INFINITE);		   // no time-out interval
	
	m_clientInfos = clientInfos;

	ReleaseMutex(m_clientInfoMutex);
}

void Heimdall::SendInfoToServer(const ClientInfo info)
{
	WaitForSingleObject(
		m_clientInfoMutex, // handle to mutex
		INFINITE);		   // no time-out interval

	m_info = info;
	m_infoNeedToBeSent = true;

	ReleaseMutex(m_clientInfoMutex);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////
//Common

unsigned int Heimdall::GetClientHash(const Client &client)
{
	return ((std::hash<u_long>()(client.si_other.sin_addr.S_un.S_addr) ^ (std::hash<u_short>()(client.si_other.sin_port) << 1)) >> 1);
}

//////////////////////////////////////////////////////////////////////////////////////////////////