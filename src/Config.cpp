#include "Config.h"
#include "AsyncLogger.h"
#include <fstream>
#include <algorithm>

bool Config::load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_INFO << "[Config] Warning: File " << filename << " not found, using defaults.";
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            std::string key = line.substr(0, delimiterPos);
            std::string value = line.substr(delimiterPos + 1);
            settings_[key] = value;
        }
    }
    
    LOG_INFO << "[Config] Successfully loaded configuration: " << filename;
    return true;
}

int Config::getInt(const std::string& key, int default_value) {
    auto it = settings_.find(key);
    if (it != settings_.end()) {
        try { return std::stoi(it->second); } catch (...) {}
    }
    return default_value;
}

std::string Config::getString(const std::string& key, const std::string& default_value) {
    auto it = settings_.find(key);
    if (it != settings_.end()) {
        return it->second;
    }
    return default_value;
}
