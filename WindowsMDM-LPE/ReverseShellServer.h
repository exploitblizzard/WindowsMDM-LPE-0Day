#include <iostream>
#pragma comment (lib, "Ws2_32.lib")
#include <WinSock2.h>
#include <strsafe.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS

void readFromSocket(SOCKET& sock);
void SpawnShell();