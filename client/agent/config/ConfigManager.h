/**
 * @file        ConfigManager.h
 * @brief       配置管理模块（Header-Only）
 * @details     所有实现都放在此头文件中，无需单独编译 .cpp
 */
#pragma once

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

#include "3rdparty/json/json.hpp"

class ConfigManager
{
public:
    static ConfigManager& getInstance()
    {
        static ConfigManager instance;
        return instance;
    }

    void setConfigPath(const std::string& path)
    {
        _configPath = path;
    }

    bool loadConfig()
    {
        std::ifstream file(_configPath);
        if (!file.is_open()){
            std::cout << "Failed to open config file: " << _configPath << std::endl;
            return false;
        }

        try{
            file >> _config;
        }catch (const nlohmann::json::parse_error& e) {
            _config = nlohmann::json::object();
            std::cout << "Failed to parse config file: " << e.what() << std::endl;
            return false;
        }

        return true;
    }

    bool saveConfig() const
    {
        std::string path = _configPath.empty() ? "./config.json" : _configPath;
        std::ofstream file(path);
        if (!file.is_open())
            return false;
        file << _config.dump(4);  
        return true;
    }

    template<typename T>
    T getValue(const std::string& pointer, const T& defaultValue = T{}) const
    {
        try {
            return _config.value(nlohmann::json::json_pointer(pointer), defaultValue);
        } catch (...) {
            return defaultValue;
        }
    }

    template<typename T>
    void setValue(const std::string& pointer, const T& value)
    {
        _config[nlohmann::json::json_pointer(pointer)] = value;
    }

    void printConfig()
    {
        std::cout << _config.dump(4) << std::endl;
    }

private:
    ConfigManager() : _configPath("./config.json") {}
    ~ConfigManager() = default;

    // 禁止拷贝
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    nlohmann::json _config;
    std::string    _configPath;
};