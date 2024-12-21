#include "Server.h"

Server::Server()
{
    Log::Init();

    m_ThreadPool.Create();
}

void Server::Start(const std::string& filesDirectory, uint16_t port)
{
    LOG_INFO_TAG("SERVER", "Starting server...");

    m_FilesDirectory = filesDirectory;
    m_Port = port;
    m_IsRunning = true;

    m_ThreadPool.Start();

    CreateListenSocket();

    LOG_INFO_TAG("SERVER", "Server started successfully!");

    Routine();
}

void Server::Stop()
{
    LOG_INFO_TAG("SERVER", "Stopping server...");

    m_IsRunning = false;

    m_ThreadPool.Shutdown();

    for (auto& taskFuture : m_UpdateIndexFutures)
        taskFuture.get();

    for (auto& taskFuture : m_ClientTasksFutures)
        taskFuture.get();

    closesocket(m_ListenSocket);
    WSACleanup();

    LOG_INFO_TAG("SERVER", "Server stopped successfully!");
}

void Server::CreateListenSocket()
{
    WSADATA wsaData{};

    int iResult{ WSAStartup(MAKEWORD(2, 2), &wsaData) };
    if (iResult != 0)
    {
        LOG_CRITICAL_TAG("SERVER", "WSAStartup failed: {0}", iResult);

        throw std::runtime_error("WSAStartup failed");
    }

    m_ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_ListenSocket == INVALID_SOCKET)
    {
        LOG_CRITICAL_TAG("SERVER", "Socket failed: {0}", WSAGetLastError());
        WSACleanup();

        throw std::runtime_error("Socket failed");
    }

    u_long mode{ 1u }; // 1u - non-blocking, 0u - blocking
    iResult = ioctlsocket(m_ListenSocket, FIONBIO, &mode);
    if (iResult == SOCKET_ERROR)
    {
        LOG_CRITICAL_TAG("SERVER", "ioctlsocket failed: {0}", WSAGetLastError());
        closesocket(m_ListenSocket);
        WSACleanup();

        throw std::runtime_error("ioctlsocket failed");
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(m_Port);

    iResult = bind(m_ListenSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress));
    if (iResult == SOCKET_ERROR)
    {
        LOG_CRITICAL_TAG("SERVER", "Bind failed: {0}", WSAGetLastError());
        closesocket(m_ListenSocket);
        WSACleanup();

        throw std::runtime_error("Bind failed");
    }

    iResult = listen(m_ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR)
    {
        LOG_CRITICAL_TAG("SERVER", "Listen failed: {0}", WSAGetLastError());
        closesocket(m_ListenSocket);
        WSACleanup();

        throw std::runtime_error("Listen failed");
    }
}

void Server::Routine()
{
    LOG_INFO_TAG("SERVER", "Starting routine...");

    while (m_IsRunning)
    {
        RemoveFinishedTasksFutures();

        // Updat the inverted index
        const std::chrono::time_point<std::chrono::steady_clock> currentTimePoint{ std::chrono::steady_clock::now() };
        m_ElapsedAfterLastIndexUpdateMS += std::chrono::duration_cast<std::chrono::milliseconds>(currentTimePoint - m_LastIndexUpdateTimePoint).count();
        if (m_ElapsedAfterLastIndexUpdateMS >= m_IndexUpdateIntervalMS && m_UpdateIndexFutures.empty())
        {
            m_LastIndexUpdateTimePoint = currentTimePoint,
            UpdateInvertedIndex();
        }

        // Accept a client
        const SOCKET clientSocket{ accept(m_ListenSocket, nullptr, nullptr) };
        if (clientSocket == INVALID_SOCKET)
        {
            const int error{ WSAGetLastError() };
            switch (error)
            {
                case WSAEWOULDBLOCK:
                    break;
                default:
                    LOG_ERROR_TAG("SERVER", "Accept failed: {0}", error);
                    break;
            }
            continue;
        }

        // Process the client
        m_ClientTasksFutures.emplace_back(m_ThreadPool.AddTask(SERVER_TASK_PRIORITY_HANDLE_CLIENT, [this, clientSocket]() { ProcessClient(clientSocket); }));
    }
}

void Server::RemoveFinishedTasksFutures()
{
    const auto taskCanBeDeletedPredicate{ []<typename T>(const std::future<T>& taskFuture) -> bool { return taskFuture.valid() ? taskFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready : true; } };

    std::erase_if(m_ClientTasksFutures, taskCanBeDeletedPredicate);
    std::erase_if(m_UpdateIndexFutures, taskCanBeDeletedPredicate);
}

void Server::UpdateInvertedIndex()
{
    std::vector<std::string> filePaths{};

    std::filesystem::recursive_directory_iterator directoryIterator{ m_FilesDirectory };
    for (const auto& directoryEntry : directoryIterator)
    {
        const std::string& filePath{ directoryEntry.path().string() };
        if (directoryEntry.is_regular_file() && !m_FileSystem.FileIsLoaded(filePath))
            filePaths.emplace_back(filePath);
    }

    const uint32_t freeWorkersCount{ std::max(m_ThreadPool.GetFreeWorkersCount(), 1u) };
    const uint32_t filesCount{ static_cast<uint32_t>(filePaths.size()) };
    const uint32_t filesPerWorkerCount{ filesCount / freeWorkersCount };

    std::atomic<uint32_t> filesProcessedCount{ 0u };

    const auto& workerFileLoadRoutine{
        [this, &filesProcessedCount, filePaths = std::move(filePaths)](uint32_t beginIndex, uint32_t filesCount) {
            for (const auto& filePath : filePaths | std::views::drop(beginIndex) | std::views::take(filesCount))
            {
                LOG_TRACE_TAG("SERVER", "Loading file: {0}", filePath);

                ++filesProcessedCount;

                const FileSystem::FileID fileID{ m_FileSystem.LoadFile(filePath) };
                m_InvertedIndex.Add(fileID, m_FileSystem.GetContent(fileID));
            }
        }
    };

    for (uint32_t i{ 0u }; i < freeWorkersCount - 1u; ++i)
        m_UpdateIndexFutures.emplace_back(m_ThreadPool.AddTask(SERVER_TASK_PRIORITY_UPDATE_INVERTED_INDEX, workerFileLoadRoutine, i * filesPerWorkerCount, filesPerWorkerCount));

    const uint32_t alreadyProcessedFilesCount{ (freeWorkersCount - 1u) * filesPerWorkerCount };
    m_UpdateIndexFutures.emplace_back(m_ThreadPool.AddTask(SERVER_TASK_PRIORITY_UPDATE_INVERTED_INDEX, workerFileLoadRoutine, alreadyProcessedFilesCount, filesCount - alreadyProcessedFilesCount));
}

void Server::ProcessClient(SOCKET clientSocket)
{
}