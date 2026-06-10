#include <iostream>
#include "Config/ConfigManager.h"

int main()
{
    ConfigManager& config = ConfigManager::getInstance();
    config.setConfigPath("./config.json");

    if (config.loadConfig()) {
        std::cout << "Load config success" << std::endl;
    } else {
        std::cout << "Load config failed" << std::endl;
    }

    config.printConfig();
    
    std::string api_key = config.getValue<std::string>("/API_Key", "API_Key_default");
    std::cout << "API_Key: " << api_key << std::endl;

    int num = config.getValue<int>("/num", 0);           
    std::cout << "num: " << num << std::endl;

    int id = config.getValue<int>("/user/id", 0);
    std::cout << "id: " << id << std::endl;

    config.setValue("/user/id", 123);
    config.saveConfig();

    return 0;
}