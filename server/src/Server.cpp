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
    LoadFiles();
    Routine();
}

void Server::Stop()
{
    LOG_INFO_TAG("SERVER", "Stopping server...");

    m_IsRunning = false;

    m_ThreadPool.Shutdown();

    for (auto& taskFuture : m_TasksFutures)
        taskFuture.get();

    closesocket(m_ListenSocket);
    WSACleanup();
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

void Server::LoadFiles()
{
    LOG_INFO_TAG("SERVER", "Creating inverted index...");

    std::filesystem::directory_iterator directoryIterator{ m_FilesDirectory };
    for (const auto& directoryEntry : directoryIterator)
    {
        if (directoryEntry.is_regular_file())
        {
            LOG_TRACE_TAG("SERVER", "Loading file: {0}", directoryEntry.path().string());

            const std::string&       filePath{ directoryEntry.path().string() };
            const FileSystem::FileID fileID{ m_FileSystem.LoadFile(filePath) };

            m_InvertedIndex.AddUnsafe(fileID, m_FileSystem.GetContent(fileID));
        }
    }

    LOG_INFO_TAG("SERVER", "Inverted index has been created");
}

void Server::Routine()
{
    LOG_INFO_TAG("SERVER", "Starting routine...");

    while (m_IsRunning)
    {
        // Removing finished tasks
        std::erase_if(m_TasksFutures, [](const std::future<void>& taskFuture) { return taskFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready; });

        // Accepting a client
        SOCKET clientSocket{ accept(m_ListenSocket, nullptr, nullptr) };
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

        // Processing the client
        m_TasksFutures.emplace_back(m_ThreadPool.AddTask(SERVER_TASK_PRIORITY_HANDLE_CLIENT, [this, clientSocket]() { ProcessClient(clientSocket); }));
    }
}

void Server::ProcessClient(SOCKET clientSocket)
{
}