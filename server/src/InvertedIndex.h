#pragma once
#include "FileSystem.h"

#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

class InvertedIndex
{
public:
    using ReadWriteLock = std::shared_mutex;
    using ReadLock = std::shared_lock<ReadWriteLock>;
    using WriteLock = std::unique_lock<ReadWriteLock>;

public:
    void Add(FileSystem::FileID fileID, std::string_view content);

    std::vector<FileSystem::FileID> Search(std::string_view query) const;

private:
    static std::vector<std::string> Tokenize(std::string_view content);
    static std::string              Normalize(const std::string_view token);

private:
    mutable ReadWriteLock m_ObjectLock{};

    std::unordered_map<std::string, std::vector<FileSystem::FileID>> m_Index;
};