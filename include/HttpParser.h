#pragma once
#include <string>

enum class ParseResult {
    SUCCESS,
    INCOMPLETE,
    ERROR
};

class HttpParser {
public:
    static ParseResult parse(const std::string& request, std::string& method, std::string& path);
    static std::string buildStaticResponse(const std::string& path);
};
