#pragma once

#include <winsock2.h>
#include <windows.h>
#include <string>

#include "ServerConfig.h"

VOID CALLBACK IoCompletionCallback(
    PTP_CALLBACK_INSTANCE Instance,
    PVOID Context,
    PVOID Overlapped,
    ULONG IoResult,
    ULONG_PTR BytesTransferred,
    PTP_IO Io
);

struct ClientContext {
    SOCKET socket;
    ServerConfig* config;
    PTP_IO ptpIo;
    char readBuffer[BUFFER_SIZE];
    char writeBuffer[BUFFER_SIZE];
    OVERLAPPED readOverlapped;
    OVERLAPPED writeOverlapped;
    std::string request;
    bool authenticated;
    bool writing;

    ClientContext(SOCKET s, ServerConfig* cfg) : socket(s), config(cfg), authenticated(false), writing(false) {
        memset(&readOverlapped, 0, sizeof(OVERLAPPED));
        memset(&writeOverlapped, 0, sizeof(OVERLAPPED));
        ptpIo = CreateThreadpoolIo((HANDLE)s, IoCompletionCallback, this, nullptr);
    }

    ~ClientContext() {
        if (ptpIo) {
            CloseThreadpoolIo(ptpIo);
        }
    }
};

const auto end_msg = std::string("\r\n\r\n");

bool Authenticate(char* user, char* pass, ServerConfig* config) {
    for (auto& u : config->users) {
        if (strcmp(u.login, user) == 0 && strcmp(u.password, pass) == 0) {
            return true;
        }
    }
}

void StartAsyncRead(ClientContext* client) {
    WSABUF wsaBuf = { BUFFER_SIZE, client->readBuffer };
    DWORD flags = 0;
    StartThreadpoolIo(client->ptpIo);
    DWORD bytesRead;
    if (WSARecv(client->socket, &wsaBuf, 1, &bytesRead, &flags, &client->readOverlapped, nullptr) == SOCKET_ERROR) {
        if (WSAGetLastError() != WSA_IO_PENDING) {
            CancelThreadpoolIo(client->ptpIo);
            delete client;
        }
    }
}

void StartAsyncWrite(ClientContext* client, const char* data, size_t len) {
    client->writing = true;
    WSABUF wsaBuf = { len, const_cast<char*>(data) };
    StartThreadpoolIo(client->ptpIo);
    DWORD bytesSent;
    if (WSASend(client->socket, &wsaBuf, 1, &bytesSent, 0, &client->writeOverlapped, nullptr) == SOCKET_ERROR) {
        if (WSAGetLastError() != WSA_IO_PENDING) {
            CancelThreadpoolIo(client->ptpIo);
            delete client;
        }
    }
}

void ProcessClientRequest(ClientContext* client) {
    if (!client->authenticated) {
        std::string line = client->request.substr(0, client->request.length());
        client->request.erase(0, client->request.length());

        char user[32], pass[32];
        if (sscanf(line.c_str(), "USER %s PASS %s", user, pass) == 2) {
            if (Authenticate(user, pass, client->config)) {
                StartAsyncWrite(client, "AUTH_OK\n", 8);
                client->authenticated = true;
            }
            else {
                StartAsyncWrite(client, "AUTH_FAIL\n", 10);
                delete client;
            }
        }
    } else {
        std::string line = client->request.substr(0, client->request.length());
        client->request.erase(0, client->request.length());

        if (line.compare(0, 4, "LIST") == 0) {
            WIN32_FIND_DATAA fd;
            char path[MAX_PATH];
            sprintf(path, "%s\\*.*", client->config->directory);

            HANDLE hFind = FindFirstFileA(path, &fd);
            if (hFind == INVALID_HANDLE_VALUE) {
                StartAsyncWrite(client, "ERROR\n", 6);
                return;
            }

            std::string response;
            do {
                response += fd.cFileName;
                response += "\n";
            } while (FindNextFileA(hFind, &fd));

            FindClose(hFind);
            response += "END_LIST\n";
            StartAsyncWrite(client, response.c_str(), response.size());
        }
        else if (line.compare(0, 4, "GET ") == 0) {
            char filePath[MAX_PATH];
            sprintf(filePath, "%s\\%s", client->config->directory, line.c_str() + 4);

            FILE* file = fopen(filePath, "rb");
            if (!file) {
                StartAsyncWrite(client, "FILE_ERR\n", 9);
                return;
            }

            fseek(file, 0, SEEK_END);
            long size = ftell(file);
            fseek(file, 0, SEEK_SET);

            char sizeMsg[32] = { 0 };
            sprintf(sizeMsg, "SIZE:%ld\n", size);
            StartAsyncWrite(client, sizeMsg, strlen(sizeMsg));

            while (true) {
                int bytesRead = fread(client->readBuffer, 1, BUFFER_SIZE, file);
                if (bytesRead <= 0) break;
                StartAsyncWrite(client, client->readBuffer, bytesRead);
            }

            fclose(file);
        }
    }
}

VOID CALLBACK IoCompletionCallback(
    PTP_CALLBACK_INSTANCE Instance,
    PVOID Context,
    PVOID Overlapped,
    ULONG IoResult,
    ULONG_PTR BytesTransferred,
    PTP_IO Io
) {
    ClientContext* client = static_cast<ClientContext*>(Context);
    OVERLAPPED* ov = static_cast<OVERLAPPED*>(Overlapped);

    if (IoResult != NO_ERROR || BytesTransferred == 0) {
        delete client;
        return;
    }

    if (ov == &client->readOverlapped) {
        client->request.append(client->readBuffer, BytesTransferred);
        ProcessClientRequest(client);
    }
    else if (ov == &client->writeOverlapped) {
        client->writing = false;
        StartAsyncRead(client);
    }
}