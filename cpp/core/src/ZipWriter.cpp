#include "native/ZipWriter.h"
#include <windows.h>
#include <fstream>

namespace steprec::core {

namespace {

uint32_t s_crcTable[256];
bool s_crcInit = false;

void InitCrcTable() {
    if (s_crcInit) return;
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j) {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        s_crcTable[i] = c;
    }
    s_crcInit = true;
}

} // anonymous namespace

uint32_t ZipWriter::CalculateCrc32(const void* data, size_t length) {
    InitCrcTable();
    uint32_t crc = ~0u;
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < length; ++i) {
        crc = s_crcTable[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

bool ZipWriter::CreateZip(const std::wstring& destinationPath,
                          const std::vector<ZipEntry>& entries,
                          std::wstring& errorMessage) {
    size_t slash = destinationPath.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        std::wstring folder = destinationPath.substr(0, slash);
        DWORD attrs = GetFileAttributesW(folder.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            errorMessage = L"The destination folder does not exist: " + folder;
            return false;
        }
    }

    std::ofstream f(destinationPath, std::ios::binary | std::ios::trunc);
    if (!f) {
        errorMessage = L"Could not open ZIP file for writing: " + destinationPath;
        return false;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    uint16_t dosTime = static_cast<uint16_t>((st.wHour << 11) | (st.wMinute << 5) | (st.wSecond / 2));
    uint16_t dosDate = static_cast<uint16_t>((((int)st.wYear - 1980) << 9) | (st.wMonth << 5) | st.wDay);

    auto writeU16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
    auto writeU32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };

    std::vector<uint32_t> localHeaderOffsets;
    localHeaderOffsets.reserve(entries.size());

    // Write Local File Headers and Data (STORE method, no compression, perfect for pre-compressed PNGs and small reports)
    for (const auto& entry : entries) {
        localHeaderOffsets.push_back(static_cast<uint32_t>(f.tellp()));
        uint32_t crc = CalculateCrc32(entry.data.data(), entry.data.size());
        uint32_t size = static_cast<uint32_t>(entry.data.size());

        writeU32(0x04034b50);                                    // Local file header signature
        writeU16(20);                                            // Version needed to extract (2.0)
        writeU16(0);                                             // General purpose bit flag
        writeU16(0);                                             // Compression method (0 = STORE)
        writeU16(dosTime);                                       // Last mod file time
        writeU16(dosDate);                                       // Last mod file date
        writeU32(crc);                                           // CRC-32
        writeU32(size);                                          // Compressed size
        writeU32(size);                                          // Uncompressed size
        writeU16(static_cast<uint16_t>(entry.name.size()));      // File name length
        writeU16(0);                                             // Extra field length
        f.write(entry.name.data(), entry.name.size());
        if (!entry.data.empty()) {
            f.write(reinterpret_cast<const char*>(entry.data.data()), entry.data.size());
        }
    }

    // Write Central Directory Headers
    uint32_t centralDirStart = static_cast<uint32_t>(f.tellp());
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        uint32_t crc = CalculateCrc32(entry.data.data(), entry.data.size());
        uint32_t size = static_cast<uint32_t>(entry.data.size());

        writeU32(0x02014b50);                                    // Central directory header signature
        writeU16(20);                                            // Version made by (2.0)
        writeU16(20);                                            // Version needed to extract (2.0)
        writeU16(0);                                             // General purpose bit flag
        writeU16(0);                                             // Compression method (0 = STORE)
        writeU16(dosTime);                                       // Last mod file time
        writeU16(dosDate);                                       // Last mod file date
        writeU32(crc);                                           // CRC-32
        writeU32(size);                                          // Compressed size
        writeU32(size);                                          // Uncompressed size
        writeU16(static_cast<uint16_t>(entry.name.size()));      // File name length
        writeU16(0);                                             // Extra field length
        writeU16(0);                                             // File comment length
        writeU16(0);                                             // Disk number start
        writeU16(0);                                             // Internal file attributes
        writeU32(0);                                             // External file attributes
        writeU32(localHeaderOffsets[i]);                         // Relative offset of local header
        f.write(entry.name.data(), entry.name.size());
    }

    uint32_t centralDirSize = static_cast<uint32_t>(f.tellp()) - centralDirStart;

    // End of Central Directory Record
    writeU32(0x06054b50);                                        // End of central dir signature
    writeU16(0);                                                 // Number of this disk
    writeU16(0);                                                 // Disk with start of central directory
    writeU16(static_cast<uint16_t>(entries.size()));             // Total entries on this disk
    writeU16(static_cast<uint16_t>(entries.size()));             // Total entries
    writeU32(centralDirSize);                                    // Size of central directory
    writeU32(centralDirStart);                                   // Offset of start of central directory
    writeU16(0);                                                 // ZIP comment length

    f.close();
    if (!f) {
        errorMessage = L"Failed to write all bytes to ZIP file. Disk may be full.";
        return false;
    }

    return true;
}

bool ZipWriter::ValidateZip(const std::wstring& zipPath,
                            size_t& outEntryCount,
                            std::wstring& errorMessage) {
    outEntryCount = 0;
    std::ifstream f(zipPath, std::ios::binary | std::ios::ate);
    if (!f) {
        errorMessage = L"Cannot open ZIP file for validation: " + zipPath;
        return false;
    }

    std::streampos fileSize = f.tellg();
    if (fileSize < 22) { // 22 bytes is minimum size for End of Central Directory
        errorMessage = L"File is too small to be a valid ZIP archive.";
        return false;
    }

    // Check PK header at start
    f.seekg(0, std::ios::beg);
    char sig[4] = { 0 };
    f.read(sig, 4);
    if (f.gcount() < 4 || sig[0] != 'P' || sig[1] != 'K') {
        errorMessage = L"Missing PK signature at beginning of ZIP file.";
        return false;
    }

    // Locate End of Central Directory Record by scanning backward from end (up to 65KB + 22 bytes for max comment)
    int64_t totalSize = static_cast<int64_t>(fileSize);
    size_t scanLength = static_cast<size_t>(totalSize > 65557 ? 65557 : totalSize);
    std::vector<char> buffer(scanLength);
    f.seekg(-static_cast<std::streamoff>(scanLength), std::ios::end);
    f.read(buffer.data(), scanLength);

    int eocdOffset = -1;
    for (int i = static_cast<int>(scanLength) - 22; i >= 0; --i) {
        if (buffer[i] == 0x50 && buffer[i + 1] == 0x4B &&
            buffer[i + 2] == 0x05 && buffer[i + 3] == 0x06) {
            eocdOffset = i;
            break;
        }
    }

    if (eocdOffset == -1) {
        errorMessage = L"End of Central Directory Record not found in ZIP.";
        return false;
    }

    const uint8_t* eocd = reinterpret_cast<const uint8_t*>(buffer.data() + eocdOffset);
    uint16_t totalEntries = static_cast<uint16_t>(eocd[10] | (eocd[11] << 8));
    outEntryCount = totalEntries;

    return true;
}

} // namespace steprec::core
