#pragma once

#include "MapObjectTypes.hpp"
#include <string>

class MapObjectConfig
{
public:
    static MapObjectConfig& Instance();

    bool Load();
    bool ReloadIfChanged();
    const MapObjectConfigData& GetData() const { return m_data; }

private:
    MapObjectConfig();
    bool ParseFile();
    static MapObjectConfigData DefaultData();

    std::string m_path;
    long long m_lastWriteTime = 0;
    bool m_hasTimestamp = false;
    MapObjectConfigData m_data;
};

