#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <shlwapi.h>
#include <shlobj_core.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Shlwapi.lib")

#define CONFIG_FILE L"client.ini"
#define BUFFER_SIZE 4096

typedef struct {
    char ip[16];
    int port;
    char directory[MAX_PATH];
    char login[32];
    char password[32];
} ClientConfig;

std::wstring pwd() {
    wchar_t path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    PathRemoveFileSpecW(path);
	return std::wstring(path);
}

void LoadConfig(ClientConfig* config) {
    wchar_t w_ip[16];
    wchar_t w_port[32];
	wchar_t w_directory[MAX_PATH];
    wchar_t w_login[32];
    wchar_t w_password[32];

	std::wstring path = pwd();
    std::wstring pathToIni = path + L"\\" + CONFIG_FILE;

    if (!GetFileAttributesW(pathToIni.c_str())) {
        printf("client.ini file not found\n");
        exit(1);
    }

    GetPrivateProfileString(L"Server", L"IP", L"127.0.0.1", w_ip, 16, pathToIni.c_str());
    GetPrivateProfileString(L"Server", L"Port", L"8080", w_port, 32, pathToIni.c_str());
	GetPrivateProfileString(L"Client", L"Directory", L".\\client_files", w_directory, MAX_PATH, pathToIni.c_str());
    GetPrivateProfileString(L"Auth", L"Login", L"windows", w_login, 32, pathToIni.c_str());
    GetPrivateProfileString(L"Auth", L"Password", L"windows", w_password, 32, pathToIni.c_str());

    wcstombs(config->ip, w_ip, 16);
    config->port = _wtoi(w_port);
	wcstombs(config->directory, w_directory, MAX_PATH);
    wcstombs(config->login, w_login, 32);
    wcstombs(config->password, w_password, 32);
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    ClientConfig config;
    LoadConfig(&config);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN addr = { AF_INET };
    addr.sin_port = htons(config.port);
    addr.sin_addr.s_addr = inet_addr(config.ip);

    connect(sock, (SOCKADDR*)&addr, sizeof(addr));

    // Authenticate
    char authMsg[128] = { 0 };
    sprintf(authMsg, "USER %s PASS %s", config.login, config.password);
    send(sock, authMsg, strlen(authMsg), 0);

    char response[32] = { 0 };
    recv(sock, response, 32, 0);
    if (strcmp(response, "AUTH_OK\n") != 0) {
        printf("Authentication failed!\n");
		system("pause");
        return 1;
    }

    while (1) {
        printf("Enter command (LIST/GET filename/EXIT): ");
        char cmd[BUFFER_SIZE];
        fgets(cmd, BUFFER_SIZE, stdin);
        cmd[strcspn(cmd, "\n")] = '\0';

        if (strcmp(cmd, "EXIT") == 0) break;

        send(sock, cmd, strlen(cmd), 0);

        if (strncmp(cmd, "LIST", 4) == 0) {
			std::string fullMessage = "";
            while (1) {
                char buffer[BUFFER_SIZE] = { 0 };
                int bytesRead = recv(sock, buffer, BUFFER_SIZE, 0);

                /*if (strcmp(buffer, "END_LIST") == 0) break;
                printf("%s", buffer);*/

				fullMessage += buffer;

                // find "END_LIST\n" in fullMessage, delete it from buffer, output result
				size_t pos = fullMessage.find("END_LIST\n");
                if (pos != std::string::npos) {
                    fullMessage.erase(pos, 8);
                    printf("%s", fullMessage.c_str());
                    break;
                }
            }
        }
        else if (strncmp(cmd, "GET ", 4) == 0) {
            wchar_t userFolder[MAX_PATH];
            wsprintf(userFolder, L"%hs\\%hs", config.directory, config.login);
            std::wstring path = pwd();
            std::wstring fullPath = path + L"\\" + std::wstring(userFolder);

            // Create intermediate directories if they do not exist
            if (!PathFileExistsW(fullPath.c_str())) {
                SHCreateDirectoryExW(NULL, fullPath.c_str(), NULL);
            }

            char sizeMsg[32];
            recv(sock, sizeMsg, 32, 0);

            if (strncmp(sizeMsg, "SIZE:", 5) != 0) {
                printf("File error!\n");
                continue;
            }

            long fileSize = atol(sizeMsg + 5);
            char filePath[MAX_PATH];
            sprintf(filePath, "%s\\%s\\%s", config.directory, config.login, cmd + 4);
            FILE* file = fopen(filePath, "wb");

            long total = 0;
            while (total < fileSize) {
                char buffer[BUFFER_SIZE];
                int bytesRead = recv(sock, buffer, BUFFER_SIZE, 0);
                fwrite(buffer, 1, bytesRead, file);
                total += bytesRead;
            }

            fclose(file);
            printf("File received: %s\n", filePath);
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
