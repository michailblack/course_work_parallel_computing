#include "Server.h"

namespace Utils
{
    namespace
    {
        void        RecvAll(SOCKET socket, char* buffer, uint32_t length);
        void        SendAll(SOCKET socket, const char* buffer, uint32_t length);
        std::string UrlDecode(std::string_view value);
    } // namespace
} // namespace Utils

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

    const auto& workerFileLoadRoutine{
        [this, filePaths = std::move(filePaths)](uint32_t beginIndex, uint32_t filesCount) {
            for (const auto& filePath : filePaths | std::views::drop(beginIndex) | std::views::take(filesCount))
            {
                // LOG_TRACE_TAG("SERVER", "Loading file: {0}", filePath);

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
    sockaddr_in clientAddr{};
    int         clientAddrSize{ sizeof(clientAddr) };
    char        clientIP[INET_ADDRSTRLEN]{};
    uint16_t    clientPort{ 0u };

    if (getpeername(clientSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrSize) == 0)
    {
        // Convert the client's IP address to a string
        inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
        clientPort = ntohs(clientAddr.sin_port);

        LOG_INFO_TAG("SERVER", "Connected to the client {0}:{1}", clientIP, clientPort);
    }
    else
    {
        closesocket(clientSocket);
        LOG_ERROR_TAG("SERVER", "Failed to get the client information: {0}\nClosed the client socket", WSAGetLastError());
        return;
    }

    try
    {
        constexpr uint32_t peekBufferSize{ 1024u };
        char               peekBuffer[peekBufferSize]{};
        int                bytesPeeked{ 0 };

        while (true)
        {
            bytesPeeked = recv(clientSocket, peekBuffer, peekBufferSize, MSG_PEEK);
            if (bytesPeeked == SOCKET_ERROR)
            {
                const int error{ WSAGetLastError() };
                switch (error)
                {
                    case WSAEWOULDBLOCK:
                        continue;
                    default:
                        throw std::runtime_error(std::format("Peek failed: {0}", error).c_str());
                }
            }
            else if (bytesPeeked == 0) // The connection has been gracefully closed
            {
                closesocket(clientSocket);
                LOG_INFO_TAG("SERVER", "Closed the client socket {0}:{1}", clientIP, clientPort);
                return;
            }
            else
            {
                break;
            }
        }

        std::string_view peekData{ peekBuffer, static_cast<size_t>(bytesPeeked) };

        bool isHTTPRequest{ false };

        using namespace std::literals;
        constexpr std::array httpMethods{ "GET"sv, "POST"sv, "PUT"sv, "DELETE"sv, "HEAD"sv, "CONNECT"sv, "OPTIONS"sv, "TRACE"sv, "PATCH"sv };

        std::ranges::any_of(httpMethods, [&isHTTPRequest, &peekData](const std::string_view& httpMethod) { return isHTTPRequest = peekData.starts_with(httpMethod); });

        if (isHTTPRequest)
            HandleHTTPClient(clientSocket);
        else
            HandleSocketClient(clientSocket);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR_TAG("SERVER", "Client {0}:{1} exception: {2}", clientIP, clientPort, e.what());
    }

    // Step 6
    // Close the client socket
    closesocket(clientSocket);
    LOG_INFO_TAG("SERVER", "Closed the client socket {0}:{1}", clientIP, clientPort);
}

void Server::HandleSocketClient(SOCKET clientSocket)
{
    while (true)
    {
        // Step 1
        // Receive the length of the query string (4 bytes, network byte order)
        uint32_t requestLength{ 0u };
        Utils::RecvAll(clientSocket, reinterpret_cast<char*>(&requestLength), sizeof(requestLength));
        const uint32_t requestLengthNetworkOrder{ htonl(requestLength) };

        if (requestLengthNetworkOrder == 0u)
            break;

        // Step 2
        // Receive the query string based on the received length
        std::string request(requestLengthNetworkOrder, '\0');
        Utils::RecvAll(clientSocket, request.data(), requestLengthNetworkOrder);

        // Step 3
        // Search the query in the inverted index
        const auto&              foundFiles{ m_InvertedIndex.Search(request) };
        std::vector<std::string> foundFilesPaths{};
        foundFilesPaths.reserve(foundFiles.size());

        std::ranges::for_each(foundFiles, [this, &foundFilesPaths](const FileSystem::FileID fileID) { foundFilesPaths.emplace_back(m_FileSystem.GetPath(fileID)); });

        // Step 4
        // Send the number of the found files (4 bytes, network byte order)
        const uint32_t resultsCount{ static_cast<uint32_t>(foundFilesPaths.size()) };
        const uint32_t resultsCountNetworkOrder{ htonl(resultsCount) };
        Utils::SendAll(clientSocket, reinterpret_cast<const char*>(&resultsCountNetworkOrder), sizeof(resultsCountNetworkOrder));

        // Step 5
        // Send each found file path
        for (const std::string& filePath : foundFilesPaths)
        {
            const uint32_t filePathLength{ static_cast<uint32_t>(filePath.size()) };
            const uint32_t filePathLengthNetworkOrder{ htonl(filePathLength) };

            Utils::SendAll(clientSocket, reinterpret_cast<const char*>(&filePathLengthNetworkOrder), sizeof(filePathLengthNetworkOrder));

            if (filePathLength == 0u)
                continue;

            Utils::SendAll(clientSocket, filePath.data(), filePathLength);
        }
    }
}

void Server::HandleHTTPClient(SOCKET clientSocket)
{
    std::string        requestStr{};
    constexpr uint32_t bufferSize{ 1024u };
    char               buffer[bufferSize]{};

    while (requestStr.find("\r\n\r\n") == std::string::npos)
    {
        int bytesReceived{ recv(clientSocket, buffer, bufferSize - 1u, 0) };
        if (bytesReceived == SOCKET_ERROR)
        {
            const int error{ WSAGetLastError() };
            switch (error)
            {
                case WSAEWOULDBLOCK:
                    continue;
                default:
                    throw std::runtime_error(std::format("Recv failed: {0}", error).c_str());
            }
        }
        else if (bytesReceived == 0) // The connection has been gracefully closed
        {
            return;
        }

        buffer[bytesReceived] = '\0';
        requestStr.append(buffer, bytesReceived);
    }

    std::istringstream requestStream{ requestStr };
    std::string        requestLine{};
    if (std::getline(requestStream, requestLine); requestLine.empty())
        throw std::runtime_error("Empty request line from HTTP request");

    if (requestLine.back() == '\r')
        requestLine.pop_back();

    std::istringstream requestLineStream{ requestLine };
    std::string        method{}, path{}, httpVersion{};
    requestLineStream >> method >> path >> httpVersion;

    if (method.empty() || method != "GET")
    {
        std::string response{ "HTTP/1.1 405 Method Not Allowed\r\n"
                              "Content-Length: 0\r\n"
                              "Connection: close\r\n\r\n" };
        Utils::SendAll(clientSocket, response.data(), response.size());
        return;
    }

    std::unordered_map<std::string, std::string> headers{};
    std::string                                  headerLine{};
    while (std::getline(requestStream, headerLine) && !headerLine.empty() && headerLine != "\r")
    {
        if (headerLine.back() == '\r')
            headerLine.pop_back();

        const size_t colonPos{ headerLine.find(':') };
        if (colonPos == std::string::npos)
            continue;

        const std::string headerName{ headerLine.substr(0u, colonPos) };
        const std::string headerValue{ headerLine.substr(colonPos + 1u) };

        headers[headerName] = headerValue.substr(headerValue.find_first_not_of(' '));
    }

    std::string  queryStr{};
    const size_t queryPos{ path.find('?') };
    if (queryPos != std::string::npos)
    {
        queryStr = path.substr(queryPos + 1u);
        path.resize(queryPos); // path = path.substr(0u, queryPos);
    }

    std::unordered_map<std::string, std::string> queryParams{};
    std::istringstream                           queryStream{ queryStr };
    std::string                                  keyValuePair{};
    while (std::getline(queryStream, keyValuePair, '&'))
    {
        const size_t equalPos{ keyValuePair.find('=') };
        if (equalPos == std::string::npos)
            continue;

        const std::string key{ keyValuePair.substr(0u, equalPos) };
        const std::string value{ keyValuePair.substr(equalPos + 1u) };

        queryParams[Utils::UrlDecode(key)] = Utils::UrlDecode(value);
    }

    if (!queryParams.contains("q"))
    {
        std::string response{ "HTTP/1.1 400 Bad Request\r\n"
                              "Content-Length: 0\r\n"
                              "Connection: close\r\n\r\n" };
        Utils::SendAll(clientSocket, response.data(), response.size());
        return;
    }

    const std::string_view   query{ queryParams["q"] };
    const auto&              foundFiles{ m_InvertedIndex.Search(query) };
    std::vector<std::string> foundFilesPaths{};
    foundFilesPaths.reserve(foundFiles.size());

    std::ranges::for_each(foundFiles, [this, &foundFilesPaths](const FileSystem::FileID fileID) { foundFilesPaths.emplace_back(m_FileSystem.GetPath(fileID)); });

    std::ostringstream jsonStream{};
    jsonStream << "{ \"results\": [";
    for (size_t i{ 0u }; i < foundFilesPaths.size(); ++i)
    {
        std::string escapedFilePath{};
        for (const char c : foundFilesPaths[i])
        {
            if (c == '\\' || c == '"')
                escapedFilePath.push_back('\\');
            escapedFilePath.push_back(c);
        }

        jsonStream << "\"" << escapedFilePath << "\"";
        if (i + 1 < foundFilesPaths.size())
            jsonStream << ", ";
    }
    jsonStream << "] }";

    const std::string jsonBody{ jsonStream.str() };

    std::ostringstream responseStream{};
    responseStream << "HTTP/1.1 200 OK\r\n"
                   << "Content-Type: application/json\r\n"
                   << "Content-Length: " << jsonBody.size() << "\r\n"
                   << "Connection: close\r\n\r\n"
                   << jsonBody;

    const std::string httpResponse{ responseStream.str() };
    Utils::SendAll(clientSocket, httpResponse.data(), httpResponse.size());
}

namespace Utils
{
    namespace
    {
        void RecvAll(SOCKET socket, char* buffer, uint32_t length)
        {
            uint32_t totalBytesReceived{ 0u };
            while (totalBytesReceived < length)
            {
                const int bytesReceived{ recv(socket, buffer + totalBytesReceived, static_cast<int>(length - totalBytesReceived), 0) };
                if (bytesReceived == SOCKET_ERROR)
                {
                    const int error{ WSAGetLastError() };
                    switch (error)
                    {
                        case WSAEWOULDBLOCK:
                            continue;
                        default:
                            throw std::runtime_error(std::format("Recv failed: {0}", error).c_str());
                    }
                }
                else if (bytesReceived == 0) // The connection has been gracefully closed
                {
                    memset(buffer, 0, length);
                    return;
                }

                totalBytesReceived += bytesReceived;
            }
        }

        void SendAll(SOCKET socket, const char* buffer, uint32_t length)
        {
            uint32_t totalBytesSent{ 0u };
            while (totalBytesSent < length)
            {
                const int bytesSent{ send(socket, buffer + totalBytesSent, static_cast<int>(length - totalBytesSent), 0) };
                if (bytesSent == SOCKET_ERROR)
                {
                    const int error{ WSAGetLastError() };
                    switch (error)
                    {
                        case WSAEWOULDBLOCK:
                            continue;
                        default:
                            throw std::runtime_error(std::format("Send failed: {0}", error).c_str());
                    }
                }

                totalBytesSent += bytesSent;
            }
        }

        std::string UrlDecode(std::string_view value)
        {
            std::string decodedValue{};
            decodedValue.reserve(value.size());

            for (size_t i{ 0u }; i < value.size(); ++i)
            {
                if (value[i] == '%')
                {
                    if (i + 2 < value.size())
                    {
                        const char hex1{ value[i + 1] };
                        const char hex2{ value[i + 2] };

                        if (isxdigit(hex1) && isxdigit(hex2))
                        {
                            const char decodedChar{ static_cast<char>(std::stoi(std::string{ hex1, hex2 }, nullptr, 16)) };
                            decodedValue.push_back(decodedChar);
                            i += 2;
                        }
                        else
                        {
                            decodedValue.push_back(value[i]);
                        }
                    }
                    else
                    {
                        decodedValue.push_back(value[i]);
                    }
                }
                else if (value[i] == '+')
                {
                    decodedValue.push_back(' ');
                }
                else
                {
                    decodedValue.push_back(value[i]);
                }
            }

            return decodedValue;
        }
    } // namespace
} // namespace Utils