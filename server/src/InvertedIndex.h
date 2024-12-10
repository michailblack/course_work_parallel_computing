#pragma once
#include "FileSystem.h"

#include <string_view>
#include <unordered_map>
#include <vector>

class InvertedIndex
{
public:
    void AddUnsafe(FileSystem::FileID fileID, std::string_view content);

    std::vector<FileSystem::FileID> SearchUnsafe(std::string_view query) const;

private:
    static std::vector<std::string> Tokenize(std::string_view content);
    static std::string              Normalize(const std::string_view token);

private:
    std::unordered_map<std::string, std::vector<FileSystem::FileID>> m_Index;
};