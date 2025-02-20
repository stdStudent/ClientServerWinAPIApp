#pragma once

#include <windows.h>

// Global thread pool and callback environment
PTP_POOL g_ThreadPool = nullptr;
TP_CALLBACK_ENVIRON g_CallbackEnv;

void InitializeThreadPool()
{
    // Create a custom thread pool
    g_ThreadPool = CreateThreadpool(nullptr);
    if (g_ThreadPool == nullptr) {
        // Handle error
    }

    // Set the thread pool to have exactly 2 threads
    SetThreadpoolThreadMaximum(g_ThreadPool, 2);
    BOOL success = SetThreadpoolThreadMinimum(g_ThreadPool, 2);
    if (!success) {
        // Handle error
    }

    // Initialize the callback environment
    InitializeThreadpoolEnvironment(&g_CallbackEnv);
    SetThreadpoolCallbackPool(&g_CallbackEnv, g_ThreadPool);
}

void CleanupThreadPool()
{
    // Destroy the callback environment
    DestroyThreadpoolEnvironment(&g_CallbackEnv);

    // Close the thread pool
    CloseThreadpool(g_ThreadPool);
}
