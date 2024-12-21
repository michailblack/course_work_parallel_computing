#pragma once
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

class FileSystem
{
public:
    using FileID = size_t;

    using ReadWriteLock = std::shared_mutex;
    using ReadLock = std::shared_lock<ReadWriteLock>;
    using WriteLock = std::unique_lock<ReadWriteLock>;

public:
    FileID LoadFile(const std::string& path);

    std::string_view GetContent(FileID fileID) const;
    std::string_view GetPath(FileID fileID) const;

    bool FileIsLoaded(FileID fileID) const;
    bool FileIsLoaded(const std::string& path) const;

private:
    mutable ReadWriteLock m_ObjectLock{};

    std::unordered_map<FileID, std::string> m_WatchedFileContents{};
    std::unordered_map<FileID, std::string> m_WatchedFilePaths{};
};