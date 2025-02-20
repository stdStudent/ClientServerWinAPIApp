#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <string>
#include <vector>
#include <shlwapi.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Shlwapi.lib")

#include "GlobalThreadPool.h"
#include "ServerConfig.h"
#include "ClientContext.h"

std::wstring pwd() {
    wchar_t path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    PathRemoveFileSpecW(path);
    return std::wstring(path);
}

void LoadConfig(ServerConfig* config) {
    wchar_t port[32];
    wchar_t directory[MAX_PATH];

    auto path = pwd();
    std::wstring pathToIni = path + L"\\" + CONFIG_FILE;

	if (!GetFileAttributesW(pathToIni.c_str())) {
		printf("server.ini file not found\n");
		exit(1);
	}

    GetPrivateProfileStringW(L"Server", L"Port", L"8080", port, 32, pathToIni.c_str());
    GetPrivateProfileStringW(L"Server", L"Directory", L".\\server_files", directory, MAX_PATH, pathToIni.c_str());

    wchar_t sectionName[32] = { 0 };
    for (int i = 1; ;++i) {
		swprintf(sectionName, L"User%d", i);

        wchar_t log[32] = { 0 };
        wchar_t pass[32] = { 0 };
		GetPrivateProfileStringW(sectionName, L"Login", L"", log, 32, pathToIni.c_str());
		GetPrivateProfileStringW(sectionName, L"Password", L"", pass, 32, pathToIni.c_str());

		if (wcslen(log) == 0 || wcslen(pass) == 0) {
			break;
		}

		char login[32] = { 0 };
		char password[32] = { 0 };
        wcstombs(login, log, 32);
        wcstombs(password, pass, 32);

        auto user = User{};
        strncpy(user.login, login, sizeof(user.login) - 1);
        strncpy(user.password, password, sizeof(user.password) - 1);
        user.login[sizeof(user.login) - 1] = '\0';
        user.password[sizeof(user.password) - 1] = '\0';

        config->users.push_back(user);
    }

    config->port = _wtoi(port);
    wcstombs(config->directory, directory, MAX_PATH);
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    ServerConfig config;
    LoadConfig(&config);

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN addr = { AF_INET };
    addr.sin_port = htons(config.port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listenSocket, (SOCKADDR*)&addr, sizeof(addr));
    listen(listenSocket, SOMAXCONN);

    InitializeThreadPool();

    while (true) {
        SOCKADDR_IN clientAddr;
        int addrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenSocket, (SOCKADDR*)&clientAddr, &addrLen);
        u_long mode = 1;
        ioctlsocket(clientSocket, FIONBIO, &mode);

        ClientContext* client = new ClientContext(clientSocket, &config);
        StartAsyncRead(client);
    }

    CleanupThreadPool();

    closesocket(listenSocket);
    WSACleanup();
    return 0;
}
