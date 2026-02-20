#pragma once

#include "ConfigStructs.hpp"

#include <string>
#include "iostream"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include <spdlog/spdlog.h>

bool loadConfig(const std::string& filePath, AppConfig& config);
spdlog::level::level_enum parseLogLevel(const std::string& levelStr);
