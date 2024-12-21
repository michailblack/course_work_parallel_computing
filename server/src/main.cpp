#include "Log.h"
#include "Server.h"

#include <exception>
#include <iostream>

int main(int argc, const char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: [server] <files_directory> <port>" << std::endl;
        return EXIT_FAILURE;
    }

    try
    {
        std::string filesDirectory{ argv[1] };
        uint16_t    port{ static_cast<uint16_t>(std::stoi(argv[2])) };

        Server server{};
        server.Start(filesDirectory, port);
    }
    catch (const std::exception& e)
    {
        LOG_CRITICAL_TAG("SERVER", "Exception: {0}", e.what());
        return EXIT_FAILURE;
    }
}