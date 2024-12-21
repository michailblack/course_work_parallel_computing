#pragma once
#include "FileSystem.h"
#include "InvertedIndex.h"
#include "ThreadPool.h"

#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>

enum ServerTaskPriority : uint8_t
{
    SERVER_TASK_PRIORITY_NONE = 0u,
    SERVER_TASK_PRIORITY_HANDLE_CLIENT,
    SERVER_TASK_PRIORITY_UPDATE_INVERTED_INDEX,
};

class Server
{
public:
    Server();
    ~Server() { Stop(); }

    Server(const Server&) noexcept = delete;
    Server(Server&&) noexcept = delete;

    Server& operator=(const Server&) noexcept = delete;
    Server& operator=(Server&&) noexcept = delete;

public:
    void Start(const std::string& filesDirectory, uint16_t port);
    void Stop();

private:
    void CreateListenSocket();
    void Routine();
    void RemoveFinishedTasksFutures();
    void UpdateInvertedIndex();
    void ProcessClient(SOCKET clientSocket);

private:
    ThreadPool    m_ThreadPool{};
    FileSystem    m_FileSystem{};
    InvertedIndex m_InvertedIndex{};

    std::vector<std::future<void>> m_ClientTasksFutures{};
    std::vector<std::future<void>> m_UpdateIndexFutures{};

    std::string m_FilesDirectory{};

    SOCKET   m_ListenSocket{ INVALID_SOCKET };
    uint16_t m_Port{ 0u };

    bool m_IsRunning{ false };

    std::chrono::time_point<std::chrono::steady_clock> m_LastIndexUpdateTimePoint{ std::chrono::steady_clock::now() };
    const uint32_t                                     m_IndexUpdateIntervalMS{ 5000u };
    uint32_t                                           m_ElapsedAfterLastIndexUpdateMS{ m_IndexUpdateIntervalMS };
};