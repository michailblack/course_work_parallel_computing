#pragma once
#include "FileSystem.h"
#include "InvertedIndex.h"
#include "ThreadPool.h"

#include <WS2tcpip.h>
#include <Winsock2.h>

enum ServerTaskPriority : uint8_t
{
    SERVER_TASK_PRIORITY_NONE = 0u,
    SERVER_TASK_PRIORITY_HANDLE_CLIENT,
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
    void LoadFiles();
    void Routine();
    void ProcessClient(SOCKET clientSocket);

private:
    ThreadPool    m_ThreadPool{};
    FileSystem    m_FileSystem{};
    InvertedIndex m_InvertedIndex{};

    std::vector<std::future<void>> m_TasksFutures{};

    std::string m_FilesDirectory{};

    SOCKET   m_ListenSocket{ INVALID_SOCKET };
    uint16_t m_Port{ 0u };

    bool m_IsRunning{ false };
};