#pragma once

#include <string>
#include <memory>
#include <unordered_map>

#include "tools/ToolBase.h"

enum Tools
{

};

class ToolsRegistry
{
public:
    static ToolsRegistry& getInstance();

private:
    std::unordered_map<std::string, std::unique_ptr<ToolBase>> _tools;
};