#include <exception>
#include <iostream>

#include "Server.h"

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <files_directory> <port>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string filesDirectory{ argv[1] };
    uint16_t    port{ static_cast<uint16_t>(std::stoi(argv[2])) };

    try
    {
        Server server{};
        server.Start(filesDirectory, port);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}