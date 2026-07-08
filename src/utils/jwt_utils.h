#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <ctime>
#include <json/json.h>

namespace TakeAwayPlatform {

// 简化版 JWT 工具（基于 HMAC-SHA256）
class JwtUtils {
public:
    // Base64 URL-safe 编码
    static std::string base64UrlEncode(const std::string& input) {
        static const char* base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string output;
        int val = 0, valb = -6;
        for (unsigned char c : input) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                output.push_back(base64Chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6) output.push_back(base64Chars[((val << 8) >> (valb + 8)) & 0x3F]);
        while (output.size() % 4) output.push_back('=');
        // URL-safe 替换
        for (char& c : output) {
            if (c == '+') c = '-';
            else if (c == '/') c = '_';
        }
        // 去除末尾的 =
        while (!output.empty() && output.back() == '=') output.pop_back();
        return output;
    }

    // Base64 URL-safe 解码
    static std::string base64UrlDecode(const std::string& input) {
        std::string s = input;
        // 补齐 =
        while (s.size() % 4) s.push_back('=');
        // URL-safe 还原
        for (char& c : s) {
            if (c == '-') c = '+';
            else if (c == '_') c = '/';
        }

        static const std::string base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string output;
        std::vector<int> T(256, -1);
        for (int i = 0; i < 64; i++) T[base64Chars[i]] = i;

        int val = 0, valb = -8;
        for (unsigned char c : s) {
            if (T[c] == -1) break;
            val = (val << 6) + T[c];
            valb += 6;
            if (valb >= 0) {
                output.push_back(char((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return output;
    }

    // HMAC-SHA256 签名
    static std::string hmacSha256(const std::string& data, const std::string& key) {
        unsigned char* digest = HMAC(EVP_sha256(),
                                      key.c_str(), key.length(),
                                      (unsigned char*)data.c_str(), data.length(),
                                      nullptr, nullptr);
        std::ostringstream oss;
        for (int i = 0; i < 32; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
        }
        return oss.str();
    }

    // 生成 JWT Token
    static std::string generateToken(int userId, int role, const std::string& secret, int expireHours = 24) {
        // Header
        Json::Value header;
        header["alg"] = "HS256";
        header["typ"] = "JWT";
        std::string headerB64 = base64UrlEncode(header.toStyledString());
        // 去除换行
        headerB64 = base64UrlEncode("{\"alg\":\"HS256\",\"typ\":\"JWT\"}");

        // Payload
        time_t now = time(nullptr);
        time_t exp = now + expireHours * 3600;

        std::ostringstream payloadStream;
        payloadStream << "{\"user_id\":" << userId
                      << ",\"role\":" << role
                      << ",\"iat\":" << now
                      << ",\"exp\":" << exp << "}";
        std::string payloadB64 = base64UrlEncode(payloadStream.str());

        // Signature
        std::string signingInput = headerB64 + "." + payloadB64;
        std::string signature = hmacSha256(signingInput, secret);
        // HMAC-SHA256 returns raw bytes, but for JWT we need base64url of the raw bytes
        // Recompute with proper approach:
        unsigned char* rawSig = HMAC(EVP_sha256(),
                                      secret.c_str(), secret.length(),
                                      (unsigned char*)signingInput.c_str(), signingInput.length(),
                                      nullptr, nullptr);
        std::string rawSigStr(reinterpret_cast<char*>(rawSig), 32);
        std::string sigB64 = base64UrlEncode(rawSigStr);

        return signingInput + "." + sigB64;
    }

    // 验证 JWT Token，返回 payload JSON（包含 user_id, role）
    // 验证失败返回空 Json::Value
    static Json::Value verifyToken(const std::string& token, const std::string& secret) {
        // 拆分 token
        size_t firstDot = token.find('.');
        size_t secondDot = token.find('.', firstDot + 1);
        if (firstDot == std::string::npos || secondDot == std::string::npos) {
            return Json::Value(Json::nullValue);
        }

        std::string headerB64 = token.substr(0, firstDot);
        std::string payloadB64 = token.substr(firstDot + 1, secondDot - firstDot - 1);
        std::string signatureB64 = token.substr(secondDot + 1);

        // 验证签名
        std::string signingInput = headerB64 + "." + payloadB64;
        unsigned char* rawSig = HMAC(EVP_sha256(),
                                      secret.c_str(), secret.length(),
                                      (unsigned char*)signingInput.c_str(), signingInput.length(),
                                      nullptr, nullptr);
        std::string rawSigStr(reinterpret_cast<char*>(rawSig), 32);
        std::string expectedSig = base64UrlEncode(rawSigStr);

        if (expectedSig != signatureB64) {
            return Json::Value(Json::nullValue);
        }

        // 解析 payload
        std::string payloadJson = base64UrlDecode(payloadB64);
        Json::Value payload;
        Json::CharReaderBuilder builder;
        std::string errors;
        std::istringstream stream(payloadJson);
        if (!Json::parseFromStream(builder, stream, &payload, &errors)) {
            return Json::Value(Json::nullValue);
        }

        // 检查过期时间
        if (payload.isMember("exp")) {
            int64_t exp = payload["exp"].asInt64();
            if (time(nullptr) > exp) {
                return Json::Value(Json::nullValue);
            }
        }

        return payload;
    }
};

} // namespace TakeAwayPlatform
