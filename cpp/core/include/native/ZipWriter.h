#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace steprec::core {

struct ZipEntry {
    std::string name;
    std::vector<uint8_t> data;
};

class ZipWriter {
public:
    static uint32_t CalculateCrc32(const void* data, size_t length);

    static bool CreateZip(const std::wstring& destinationPath,
                          const std::vector<ZipEntry>& entries,
                          std::wstring& errorMessage);

    static bool ValidateZip(const std::wstring& zipPath,
                           size_t& outEntryCount,
                           std::wstring& errorMessage);
};

} // namespace steprec::core
