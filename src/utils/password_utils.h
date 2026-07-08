#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <random>
#include <openssl/sha.h>

namespace TakeAwayPlatform {

// 密码工具：SHA-256 加盐哈希
class PasswordUtils {
public:
    // 生成随机盐值（16字节十六进制字符串）
    static std::string generateSalt() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        std::ostringstream oss;
        for (int i = 0; i < 16; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
        }
        return oss.str();
    }

    // 计算 SHA-256 哈希
    static std::string sha256(const std::string& input) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, input.c_str(), input.length());
        SHA256_Final(hash, &sha256);

        std::ostringstream oss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return oss.str();
    }

    // 哈希密码，格式：salt$hash
    static std::string hashPassword(const std::string& plainPassword) {
        std::string salt = generateSalt();
        std::string hash = sha256(salt + plainPassword);
        return salt + "$" + hash;
    }

    // 验证密码
    static bool verifyPassword(const std::string& plainPassword, const std::string& storedHash) {
        size_t pos = storedHash.find('$');
        if (pos == std::string::npos) {
            return false;
        }
        std::string salt = storedHash.substr(0, pos);
        std::string expectedHash = salt + "$" + sha256(salt + plainPassword);
        return expectedHash == storedHash;
    }
};

} // namespace TakeAwayPlatform
