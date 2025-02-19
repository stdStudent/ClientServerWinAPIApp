#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <string>
#include <shlwapi.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Shlwapi.lib")

#define CONFIG_FILE L"server.ini"
#define BUFFER_SIZE 4096

typedef struct {
    int port;
    char directory[MAX_PATH];
    char login[32];
    char password[32];
} ServerConfig;

void LoadConfig(ServerConfig* config) {
    wchar_t port[32];
    wchar_t directory[MAX_PATH];
    wchar_t login[32];
    wchar_t password[32];

    wchar_t path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    PathRemoveFileSpecW(path);
    std::wstring pathToIni = std::wstring(path) + L"\\" + CONFIG_FILE;

	if (!GetFileAttributesW(pathToIni.c_str())) {
		printf("server.ini file not found\n");
		exit(1);
	}

    GetPrivateProfileStringW(L"Server", L"Port", L"8080", port, 32, pathToIni.c_str());
    GetPrivateProfileStringW(L"Server", L"Directory", L".\\server_files", directory, MAX_PATH, pathToIni.c_str());
    GetPrivateProfileStringW(L"Auth", L"Login", L"windows", login, 32, pathToIni.c_str());
    GetPrivateProfileStringW(L"Auth", L"Password", L"windows", password, 32, pathToIni.c_str());

    config->port = _wtoi(port);
    wcstombs(config->directory, directory, MAX_PATH);
    wcstombs(config->login, login, 32);
    wcstombs(config->password, password, 32);
}

BOOL Authenticate(char* user, char* pass, ServerConfig* config) {
    if (strcmp(config->login, "windows") == 0) {
        HANDLE hToken;
        if (LogonUserA(user, ".", pass, LOGON32_LOGON_NETWORK, LOGON32_PROVIDER_DEFAULT, &hToken)) {
            CloseHandle(hToken);
            return TRUE;
        }
        return FALSE;
    }
    return strcmp(user, config->login) == 0 && strcmp(pass, config->password) == 0;
}

DWORD WINAPI ClientHandler(LPVOID lpParam) {
    SOCKET client = *(SOCKET*)lpParam;
    char buffer[BUFFER_SIZE];
    int bytesRead;
    ServerConfig config;

    LoadConfig(&config);

    // Authentication
    bytesRead = recv(client, buffer, BUFFER_SIZE, 0);
    buffer[bytesRead] = '\0';

    char user[32], pass[32];
    sscanf(buffer, "USER %s PASS %s", user, pass);

    if (!Authenticate(user, pass, &config)) {
        send(client, "AUTH_FAIL\0", 10, 0);
        closesocket(client);
        return 0;
    }
    send(client, "AUTH_OK\0", 8, 0);

    // Command handling
    while ((bytesRead = recv(client, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytesRead] = '\0';

        if (strncmp(buffer, "LIST", 4) == 0) {
            WIN32_FIND_DATAA fd;
            char path[MAX_PATH];
            sprintf(path, "%s\\*.*", config.directory);

            HANDLE hFind = FindFirstFileA(path, &fd);
            if (hFind == INVALID_HANDLE_VALUE) {
                send(client, "ERROR\0", 6, 0);
                continue;
            }

            do {
                send(client, fd.cFileName, strlen(fd.cFileName), 0);
                send(client, "\n", 1, 0);
            } while (FindNextFileA(hFind, &fd));

            FindClose(hFind);
            send(client, "END_LIST\0", 9, 0);
        }
        else if (strncmp(buffer, "GET ", 4) == 0) {
            char filePath[MAX_PATH];
            sprintf(filePath, "%s\\%s", config.directory, buffer + 4);

            FILE* file = fopen(filePath, "rb");
            if (!file) {
                send(client, "FILE_ERR\0", 9, 0);
                continue;
            }

            fseek(file, 0, SEEK_END);
            long size = ftell(file);
            fseek(file, 0, SEEK_SET);

            char sizeMsg[32] = { 0 };
            sprintf(sizeMsg, "SIZE:%ld", size);
            send(client, sizeMsg, strlen(sizeMsg), 0);

            while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
                send(client, buffer, bytesRead, 0);
            }

            fclose(file);
        }
    }

    closesocket(client);
    return 0;
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

    while (1) {
        SOCKADDR_IN clientAddr;
        int addrLen = sizeof(clientAddr);
        SOCKET client = accept(listenSocket, (SOCKADDR*)&clientAddr, &addrLen);

        HANDLE thread = CreateThread(NULL, 0, ClientHandler, &client, 0, NULL);
        CloseHandle(thread);
    }

    closesocket(listenSocket);
    WSACleanup();
    return 0;
}
