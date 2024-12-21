#include <cstdint>
#include <format>
#include <iostream>
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace Utils
{
    namespace
    {
        void RecvAll(SOCKET socket, char* buffer, uint32_t length);
        void SendAll(SOCKET socket, const char* buffer, uint32_t length);
    } // namespace
} // namespace Utils

int main(int argc, const char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: [client] <Server IP> <Port>" << std::endl;
        return 1;
    }

    std::string serverIP{ argv[1] };
    uint16_t    port = static_cast<uint16_t>(std::stoi(argv[2]));

    WSADATA wsaData{};

    int iResult{ WSAStartup(MAKEWORD(2, 2), &wsaData) };
    if (iResult != 0)
    {
        std::cerr << "WSAStartup failed with error: " << iResult << std::endl;
        return 1;
    }

    const SOCKET connectSocket{ socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) };
    if (connectSocket == INVALID_SOCKET)
    {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0)
    {
        std::cerr << "Invalid server IP address format." << std::endl;
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }

    iResult = connect(connectSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
    if (iResult == SOCKET_ERROR)
    {
        std::cerr << "Connect failed with error: " << WSAGetLastError() << std::endl;
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server " << serverIP << ":" << port << std::endl;

    try
    {
        while (true)
        {
            std::cout << "\nEnter your query (or type 'exit' to quit): ";
            std::string query{};
            std::getline(std::cin, query);

            if (query.empty())
            {
                std::cout << "Query cannot be empty. Please try again." << std::endl;
                continue;
            }

            if (query == "exit")
                break;

            // Step 1
            // Send the length of the query string (4 bytes, network byte order)
            const uint32_t queryLength{ static_cast<uint32_t>(query.size()) };
            const uint32_t queryLengthNetworkOrder{ htonl(queryLength) };

            Utils::SendAll(connectSocket, reinterpret_cast<const char*>(&queryLengthNetworkOrder), sizeof(queryLengthNetworkOrder));

            // Step 2
            // Send the query string
            Utils::SendAll(connectSocket, query.c_str(), queryLength);

            std::cout << "Query sent: " << query << std::endl;

            // Step 4
            // Receive the number of the found files (4 bytes, network byte order)
            uint32_t resultsCountNetworkOrder{ 0u };
            Utils::RecvAll(connectSocket, reinterpret_cast<char*>(&resultsCountNetworkOrder), sizeof(resultsCountNetworkOrder));
            const uint32_t resultsCount{ ntohl(resultsCountNetworkOrder) };

            std::cout << "Number of results: " << resultsCount << std::endl;

            // Step 5
            // Receive each found file path
            std::vector<std::string> results{};
            for (uint32_t i{ 0u }; i < resultsCount; ++i)
            {
                uint32_t resultLengthNetworkOrder{ 0u };
                Utils::RecvAll(connectSocket, reinterpret_cast<char*>(&resultLengthNetworkOrder), sizeof(resultLengthNetworkOrder));
                const uint32_t resultLength{ ntohl(resultLengthNetworkOrder) };

                if (resultLength == 0u)
                    continue;

                std::string result(resultLength, '\0');
                Utils::RecvAll(connectSocket, result.data(), resultLength);

                results.emplace_back(std::move(result));
            }

            // Display the results
            std::cout << "Received Results:" << std::endl;
            for (uint32_t i{ 0u }; i < results.size(); ++i)
                std::cout << "  [" << (i + 1) << "] " << results[i] << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    // Step 6
    // Close the socket
    closesocket(connectSocket);
    WSACleanup();

    std::cout << "Disconnected from server." << std::endl;
    return 0;
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

    } // namespace
} // namespace Utils