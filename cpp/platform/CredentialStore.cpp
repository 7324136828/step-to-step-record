#include "CredentialStore.h"
#include <wincrypt.h>

#pragma comment(lib, "crypt32.lib")

namespace steprec::platform {

bool CredentialStore::ProtectData(const std::vector<uint8_t>& plaintext, std::vector<uint8_t>& outEncrypted) {
    if (plaintext.empty()) return true;

    DATA_BLOB inBlob;
    inBlob.pbData = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(plaintext.data()));
    inBlob.cbData = static_cast<DWORD>(plaintext.size());

    DATA_BLOB outBlob;
    if (!CryptProtectData(&inBlob, L"StepRecorderData", nullptr, nullptr, nullptr, 0, &outBlob)) {
        return false;
    }

    outEncrypted.assign(outBlob.pbData, outBlob.pbData + outBlob.cbData);
    LocalFree(outBlob.pbData);
    return true;
}

bool CredentialStore::UnprotectData(const std::vector<uint8_t>& encrypted, std::vector<uint8_t>& outPlaintext) {
    if (encrypted.empty()) return true;

    DATA_BLOB inBlob;
    inBlob.pbData = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(encrypted.data()));
    inBlob.cbData = static_cast<DWORD>(encrypted.size());

    DATA_BLOB outBlob;
    if (!CryptUnprotectData(&inBlob, nullptr, nullptr, nullptr, nullptr, 0, &outBlob)) {
        return false;
    }

    outPlaintext.assign(outBlob.pbData, outBlob.pbData + outBlob.cbData);
    LocalFree(outBlob.pbData);
    return true;
}

} // namespace steprec::platform
