#include "InvertedIndex.h"

void InvertedIndex::Add(FileSystem::FileID fileID, std::string_view content)
{
    std::vector<std::string> tokens{ Tokenize(content) };

    tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());
    std::sort(tokens.begin(), tokens.end());

    {
        WriteLock _{ m_ObjectLock };

        for (const auto& token : tokens)
            m_Index[token].push_back(fileID);
    }
}

std::vector<FileSystem::FileID> InvertedIndex::Search(std::string_view query) const
{
    std::vector<std::string> tokens{ Tokenize(query) };

    tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());

    std::unordered_map<FileSystem::FileID, uint32_t> filesOccurenceCount{};

    {
        ReadLock _{ m_ObjectLock };

        for (const auto& token : tokens)
        {
            const auto it{ m_Index.find(token) };

            if (it == m_Index.end())
                continue;

            const std::vector<FileSystem::FileID>& fileIDs{ it->second };

            for (const auto& fileID : fileIDs)
            {
                if (filesOccurenceCount.find(fileID) == filesOccurenceCount.end())
                    filesOccurenceCount[fileID] = 1;
                else
                    ++filesOccurenceCount[fileID];
            }
        }
    }

    std::vector<std::pair<FileSystem::FileID, uint32_t>> filesOccurenceCountVector{ filesOccurenceCount.begin(), filesOccurenceCount.end() };
    std::sort(filesOccurenceCountVector.begin(), filesOccurenceCountVector.end(), [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });

    std::vector<FileSystem::FileID> result{};
    result.reserve(filesOccurenceCountVector.size());

    for (const auto& [fileID, _] : filesOccurenceCountVector)
        result.push_back(fileID);

    return result;
}

std::vector<std::string> InvertedIndex::Tokenize(std::string_view content)
{
    std::vector<std::string> tokens{};
    tokens.reserve(std::count_if(content.begin(), content.end(), [](char c) { return std::isspace(static_cast<unsigned char>(c)); }) + 1u);

    for (size_t i{ 0u }, j{ 0u }; j < content.size(); ++j)
    {
        if (std::isspace(static_cast<unsigned char>(content[j])))
        {
            if (j > i)
                tokens.push_back(Normalize(content.substr(i, j - i)));

            i = j + 1;
        }
    }

    if (content.size() > 0 && !std::isspace(static_cast<unsigned char>(content.back())))
        tokens.push_back(Normalize(content.substr(content.find_last_of(' ') + 1u)));

    return tokens;
}

std::string InvertedIndex::Normalize(const std::string_view token)
{
    std::string normalizedToken{ token };

    std::erase_if(normalizedToken, [](char c) { return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')); });
    std::transform(normalizedToken.begin(), normalizedToken.end(), normalizedToken.begin(), [](char c) { return static_cast<char>(std::tolower(c)); });

    return normalizedToken;
}
