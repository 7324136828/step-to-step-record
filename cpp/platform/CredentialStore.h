#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace steprec::platform {

class CredentialStore {
public:
    static bool ProtectData(const std::vector<uint8_t>& plaintext, std::vector<uint8_t>& outEncrypted);
    static bool UnprotectData(const std::vector<uint8_t>& encrypted, std::vector<uint8_t>& outPlaintext);
};

} // namespace steprec::platform
