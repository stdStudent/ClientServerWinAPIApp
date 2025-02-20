#pragma once

#include <windows.h>
#include <vector>

#define CONFIG_FILE L"server.ini"
#define BUFFER_SIZE 4096

typedef struct {
	char login[32];
	char password[32];
} User;

typedef struct {
	int port;
	char directory[MAX_PATH];
	std::vector<User> users;
} ServerConfig;