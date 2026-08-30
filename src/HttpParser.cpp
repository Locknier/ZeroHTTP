#include "HttpParser.h"
#include <sstream>
#include <fstream>
#include <algorithm>

ParseResult HttpParser::parse(const std::string& request, std::string& method, std::string& path) {
    if (request.find("\r\n\r\n") == std::string::npos) {
        return ParseResult::INCOMPLETE;
    }

    std::istringstream stream(request);
    std::string line;
    
    if (std::getline(stream, line)) {
        size_t first_space = line.find(' ');
        if (first_space == std::string::npos) return ParseResult::ERROR;
        method = line.substr(0, first_space);
        
        size_t second_space = line.find(' ', first_space + 1);
        if (second_space == std::string::npos) return ParseResult::ERROR;
        path = line.substr(first_space + 1, second_space - first_space - 1);
        
        return ParseResult::SUCCESS;
    }
    
    return ParseResult::ERROR;
}

static std::string getMimeType(const std::string& path) {
    if (path.find(".html") != std::string::npos) return "text/html";
    if (path.find(".css") != std::string::npos) return "text/css";
    if (path.find(".js") != std::string::npos) return "application/javascript";
    if (path.find(".png") != std::string::npos) return "image/png";
    if (path.find(".jpg") != std::string::npos || path.find(".jpeg") != std::string::npos) return "image/jpeg";
    return "text/plain";
}

std::string HttpParser::buildStaticResponse(const std::string& path) {
    std::string safe_path = path;
    if (safe_path == "/") safe_path = "/index.html";
    
    if (safe_path.find("..") != std::string::npos) {
        return "HTTP/1.1 403 Forbidden\r\nContent-Length: 9\r\n\r\nForbidden";
    }

    std::string filepath = "./www" + safe_path;
    std::ifstream file(filepath, std::ios::binary);
    std::ostringstream response;

    if (file) {
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string body(size, '\0');
        file.read(&body[0], size);

        response << "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: " << getMimeType(safe_path) << "\r\n";
        response << "Content-Length: " << size << "\r\n";
        response << "Connection: keep-alive\r\n\r\n" << body;
    } else {
        std::ifstream notFoundFile("./www/404.html", std::ios::binary);
        if (notFoundFile) {
            notFoundFile.seekg(0, std::ios::end);
            size_t size = notFoundFile.tellg();
            notFoundFile.seekg(0, std::ios::beg);
            std::string body(size, '\0');
            notFoundFile.read(&body[0], size);

            response << "HTTP/1.1 404 Not Found\r\n";
            response << "Content-Type: text/html\r\n";
            response << "Content-Length: " << size << "\r\n";
            response << "Connection: keep-alive\r\n\r\n" << body;
        } else {
            std::string body = "<h1>404 Not Found</h1>";
            response << "HTTP/1.1 404 Not Found\r\n";
            response << "Content-Type: text/html\r\n";
            response << "Content-Length: " << body.length() << "\r\n";
            response << "Connection: keep-alive\r\n\r\n" << body;
        }
    }
    return response.str();
}
