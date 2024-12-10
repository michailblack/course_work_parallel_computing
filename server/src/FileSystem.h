#pragma once
#include <string>
#include <unordered_map>

class FileSystem
{
public:
    using FileID = size_t;

public:
    FileID                 LoadFile(const std::string& path);
    const std::string_view GetContent(FileID fileID) const;
    const std::string_view GetPath(FileID fileID) const;

private:
    std::unordered_map<FileID, std::string> m_WatchedFileContents{};
    std::unordered_map<FileID, std::string> m_WatchedFilePaths{};
};