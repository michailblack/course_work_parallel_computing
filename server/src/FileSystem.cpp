#include "FileSystem.h"

#include "Log.h"

#include <fstream>

FileSystem::FileID FileSystem::LoadFile(const std::string& path)
{
    std::ifstream fileStream{ path, std::ios::in | std::ios::ate };

    if (!fileStream.is_open())
    {
        LOG_ERROR_TAG("FileSystem", "Failed to open file: {0}", path);
        return 0u;
    }

    const size_t fileSize{ static_cast<size_t>(fileStream.tellg()) };
    fileStream.seekg(0u, std::ios::beg);

    std::string fileContent(fileSize, '\0');
    fileStream.read(fileContent.data(), fileSize);

    fileStream.close();

    const FileSystem::FileID fileID{ std::hash<std::string>{}(path) };
    m_WatchedFileContents[fileID] = fileContent;
    m_WatchedFilePaths[fileID] = path;

    return fileID;
}

const std::string_view FileSystem::GetContent(FileID fileID) const
{
    auto it{ m_WatchedFileContents.find(fileID) };
    if (it != m_WatchedFileContents.end())
        return it->second;

    return {};
}

const std::string_view FileSystem::GetPath(FileID fileID) const
{
    auto it{ m_WatchedFilePaths.find(fileID) };
    if (it != m_WatchedFilePaths.end())
        return it->second;

    return {};
}