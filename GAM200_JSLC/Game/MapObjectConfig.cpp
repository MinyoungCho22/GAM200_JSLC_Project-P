//MapObjectConfig.cpp

#include "MapObjectConfig.hpp"
#include "../Engine/Logger.hpp"
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <regex>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#endif

namespace
{
namespace fs = std::filesystem;

std::string ResolveMapObjectConfigPathString()
{
    try
    {
#ifdef _WIN32
        wchar_t buf[MAX_PATH];
        const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (n > 0 && n < MAX_PATH)
        {
            const fs::path exeDir = fs::path(buf).parent_path();
            const fs::path configJson = exeDir / "Config" / "map_objects.json";
            if (fs::exists(configJson))
                return configJson.string();
        }
#elif defined(__linux__)
        char exeBuf[PATH_MAX];
        const ssize_t n = readlink("/proc/self/exe", exeBuf, sizeof(exeBuf) - 1);
        if (n > 0)
        {
            exeBuf[n] = '\0';
            const fs::path exeDir = fs::path(exeBuf).parent_path();
            const fs::path configJson = exeDir / "Config" / "map_objects.json";
            if (fs::exists(configJson))
                return configJson.string();
        }
#endif
        if (fs::exists("Config/map_objects.json"))
            return "Config/map_objects.json";
    }
    catch (...) {}
    return "Config/map_objects.json";
}

Math::Vec2 ParseVec2(const std::string& objText, const std::string& key, Math::Vec2 fallback)
{
    std::regex rgx("\"" + key + "\"\\s*:\\s*\\[\\s*([-0-9\\.]+)\\s*,\\s*([-0-9\\.]+)\\s*\\]");
    std::smatch m;
    if (!std::regex_search(objText, m, rgx)) return fallback;
    return { std::stof(m[1].str()), std::stof(m[2].str()) };
}

std::string ParseString(const std::string& objText, const std::string& key, const std::string& fallback)
{
    std::regex rgx("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch m;
    if (!std::regex_search(objText, m, rgx)) return fallback;
    return m[1].str();
}

float ParseFloatKey(const std::string& objText, const std::string& key, float fallback)
{
    std::regex rgx("\"" + key + "\"\\s*:\\s*([-0-9\\.eE+]+)");
    std::smatch m;
    if (!std::regex_search(objText, m, rgx)) return fallback;
    return std::stof(m[1].str());
}

int ParseIntKey(const std::string& objText, const std::string& key, int fallback)
{
    std::regex rgx("\"" + key + "\"\\s*:\\s*([-0-9]+)");
    std::smatch m;
    if (!std::regex_search(objText, m, rgx)) return fallback;
    return std::stoi(m[1].str());
}

bool ParseBoolKey(const std::string& objText, const std::string& key, bool fallbackIfMissing)
{
    std::regex rgx("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch m;
    if (!std::regex_search(objText, m, rgx)) return fallbackIfMissing;
    return m[1].str() == "true";
}

Math::Vec2 ParseVec2Direct(const std::string& text, Math::Vec2 fallback)
{
    std::regex rgx("\\[\\s*([-0-9\\.]+)\\s*,\\s*([-0-9\\.]+)\\s*\\]");
    std::smatch m;
    if (!std::regex_search(text, m, rgx)) return fallback;
    return { std::stof(m[1].str()), std::stof(m[2].str()) };
}

std::string ExtractObjectByKey(const std::string& text, const std::string& key)
{
    const std::string k = "\"" + key + "\"";
    size_t pos = text.find(k);
    if (pos == std::string::npos) return {};
    pos = text.find('{', pos);
    if (pos == std::string::npos) return {};
    int depth = 0;
    size_t start = pos;
    for (size_t i = pos; i < text.size(); ++i)
    {
        if (text[i] == '{') ++depth;
        else if (text[i] == '}')
        {
            --depth;
            if (depth == 0) return text.substr(start, i - start + 1);
        }
    }
    return {};
}

std::string ExtractArrayByKey(const std::string& text, const std::string& key)
{
    const std::string k = "\"" + key + "\"";
    size_t pos = text.find(k);
    if (pos == std::string::npos) return {};
    pos = text.find('[', pos);
    if (pos == std::string::npos) return {};
    int depth = 0;
    size_t start = pos;
    for (size_t i = pos; i < text.size(); ++i)
    {
        if (text[i] == '[') ++depth;
        else if (text[i] == ']')
        {
            --depth;
            if (depth == 0) return text.substr(start, i - start + 1);
        }
    }
    return {};
}

std::vector<std::string> SplitTopLevelObjectsInArray(const std::string& arrayText)
{
    std::vector<std::string> out;
    if (arrayText.empty()) return out;
    int depth = 0;
    size_t objStart = std::string::npos;
    for (size_t i = 0; i < arrayText.size(); ++i)
    {
        if (arrayText[i] == '{')
        {
            if (depth == 0) objStart = i;
            ++depth;
        }
        else if (arrayText[i] == '}')
        {
            --depth;
            if (depth == 0 && objStart != std::string::npos)
            {
                out.push_back(arrayText.substr(objStart, i - objStart + 1));
                objStart = std::string::npos;
            }
        }
    }
    return out;
}

SpriteRectConfig ParseSpriteRect(const std::string& objText, const SpriteRectConfig& fallback)
{
    SpriteRectConfig out = fallback;
    out.topLeft = ParseVec2(objText, "top_left", fallback.topLeft);
    out.size = ParseVec2(objText, "size", fallback.size);
    out.fallbackSize = ParseVec2(objText, "fallback_size", fallback.fallbackSize);
    out.spritePath = ParseString(objText, "sprite", fallback.spritePath);
    out.hitboxMargin = ParseFloatKey(objText, "hitbox_margin", fallback.hitboxMargin);
    out.sharedPulseGroup = ParseIntKey(objText, "shared_pulse_group", fallback.sharedPulseGroup);
    out.gaugeAnchor = ParseBoolKey(objText, "gauge_anchor", fallback.gaugeAnchor);
    out.leaderRightGap = ParseFloatKey(objText, "leader_right_gap", fallback.leaderRightGap);
    out.layoutOffsetX = ParseFloatKey(objText, "layout_offset_x", fallback.layoutOffsetX);
    return out;
}

long long GetFileWriteTime(const std::string& path)
{
#ifdef _WIN32
    struct _stat st {};
    if (_stat(path.c_str(), &st) != 0) return 0;
#else
    struct stat st {};
    if (stat(path.c_str(), &st) != 0) return 0;
#endif
    // Combine modified time and file size to reduce same-second save misses.
    return (static_cast<long long>(st.st_mtime) << 32) ^ static_cast<long long>(st.st_size);
}
} // namespace

MapObjectConfig& MapObjectConfig::Instance()
{
    static MapObjectConfig instance;
    return instance;
}

MapObjectConfig::MapObjectConfig()
    : m_path("Config/map_objects.json"), m_data(DefaultData())
{
}

MapObjectConfigData MapObjectConfig::DefaultData()
{
    MapObjectConfigData data;

    data.room.pulseSources = {
        { {424.0f, 360.0f}, {51.0f, 63.0f}, "", {} },
        { {692.0f, 550.0f}, {215.0f, 180.0f}, "", {} },
        { {1414.0f, 212.0f}, {75.0f, 33.0f}, "", {} }
    };
    data.room.blind = { {1105.0f, 352.0f}, {310.0f, 300.0f}, "", {} };

    data.hallway.pulseSources = {
        { {2454.0f, 705.0f}, {210.0f, 312.0f}, "Asset/Hallway_pulsesource.png", {} }
    };
    data.hallway.hidingSpots = {
        { {2820.0f, 923.0f}, {381.0f, 324.0f}, "Asset/HidingSpot.png", {} },
        { {5620.0f, 923.0f}, {381.0f, 324.0f}, "Asset/HidingSpot.png", {} }
    };
    data.hallway.obstacle = { {7489.0f, 973.0f}, {369.0f, 645.0f}, "", {} };

    data.rooftop.hole = { {9951.0f, 1506.0f}, {785.0f, 172.0f}, "Asset/Hole.png", {} };
    data.rooftop.pulseSources = {
        { {12521.0f, 1700.0f}, {333.0f, 240.0f}, "Asset/Hvac.png", {} },
        { {12900.0f, 1700.0f}, {333.0f, 240.0f}, "Asset/Hvac.png", {} }
    };
    data.rooftop.liftButton = { {13598.0f, 1570.0f}, {}, "Asset/LiftBotton.png", {320.0f, 96.0f} };

    data.underground.robotSpawns = { {18239.0f, -1685.0f}, {21060.0f, -1685.0f} };
    data.underground.pulseSources = {
        { {1949.0f, 309.0f}, {69.0f, 255.0f}, "Asset/Underground_Pulse.png", {}, 28.0f, 0, true, -1.0f, 0.0f },
        { {4485.0f, 309.0f}, {69.0f, 255.0f}, "Asset/Underground_Pulse.png", {}, 28.0f, 0, true, -1.0f, 0.0f },
        { {5847.0f, 375.0f}, {0.0f, 0.0f}, "Asset/disco_part1.png", {}, 0.0f, 101, false, -1.0f, 0.0f },
        { {5847.0f, 375.0f}, {0.0f, 0.0f}, "Asset/disco_part2.png", {}, 0.0f, 101, true, 123.0f, 0.0f }
    };
    data.underground.obstacles = {
        { {939.0f, 834.0f}, {561.0f, 162.0f}, "", {} },
        { {1584.0f, 627.0f}, {288.0f, 369.0f}, "Asset/Pannel1.png", {} },
        { {3471.0f, 834.0f}, {561.0f, 162.0f}, "", {} },
        { {4116.0f, 627.0f}, {288.0f, 369.0f}, "Asset/pannel2.png", {} },
        { {5235.0f, 834.0f}, {561.0f, 162.0f}, "", {} },
        { {6825.0f, 627.0f}, {296.0f, 369.0f}, "", {} }
    };
    data.underground.ramps = {
        { {7140.0f, 790.0f}, {780.0f, 300.0f}, "", {} }
    };
    data.underground.lights = {
        { {0.0f, 0.0f}, {0.0f, 0.0f}, "Asset/Light.png", {} }
    };

    data.train.pulseSources = {
        { {1400.0f, 171.0f}, {270.0f, 258.0f}, "", {} },
        { {3759.0f, 84.0f}, {350.0f, 114.0f}, "", {} }
    };

    return data;
}

bool MapObjectConfig::ParseFile()
{
    std::ifstream ifs(m_path);
    if (!ifs.is_open())
    {
        Logger::Instance().Log(Logger::Severity::Info,
            "MapObjectConfig: failed to open %s (using previous/default values)",
            m_path.c_str());
        return false;
    }

    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string text = ss.str();

    MapObjectConfigData next = m_data;

    const std::string hallwayObj = ExtractObjectByKey(text, "hallway");
    if (!hallwayObj.empty())
    {
        const std::string pulseArray = ExtractArrayByKey(hallwayObj, "pulse_sources");
        if (!pulseArray.empty())
        {
            next.hallway.pulseSources.clear();
            for (const auto& obj : SplitTopLevelObjectsInArray(pulseArray))
            {
                SpriteRectConfig fallback{};
                fallback.spritePath = "Asset/Hallway_pulsesource.png";
                next.hallway.pulseSources.push_back(ParseSpriteRect(obj, fallback));
            }
        }
        const std::string hidingArray = ExtractArrayByKey(hallwayObj, "hiding_spots");
        if (!hidingArray.empty())
        {
            next.hallway.hidingSpots.clear();
            for (const auto& obj : SplitTopLevelObjectsInArray(hidingArray))
            {
                SpriteRectConfig fallback{};
                fallback.spritePath = "Asset/HidingSpot.png";
                next.hallway.hidingSpots.push_back(ParseSpriteRect(obj, fallback));
            }
        }
        const std::string obstacleObj = ExtractObjectByKey(hallwayObj, "obstacle");
        if (!obstacleObj.empty())
        {
            next.hallway.obstacle = ParseSpriteRect(obstacleObj, next.hallway.obstacle);
        }
    }

    const std::string roomObj = ExtractObjectByKey(text, "room");
    if (!roomObj.empty())
    {
        const std::string pulseArray = ExtractArrayByKey(roomObj, "pulse_sources");
        if (!pulseArray.empty())
        {
            next.room.pulseSources.clear();
            for (const auto& obj : SplitTopLevelObjectsInArray(pulseArray))
                next.room.pulseSources.push_back(ParseSpriteRect(obj, SpriteRectConfig{}));
        }
        const std::string blindObj = ExtractObjectByKey(roomObj, "blind");
        if (!blindObj.empty())
            next.room.blind = ParseSpriteRect(blindObj, next.room.blind);
    }

    const std::string rooftopObj = ExtractObjectByKey(text, "rooftop");
    if (!rooftopObj.empty())
    {
        const std::string holeObj = ExtractObjectByKey(rooftopObj, "hole");
        if (!holeObj.empty())
            next.rooftop.hole = ParseSpriteRect(holeObj, next.rooftop.hole);

        const std::string pulseArray = ExtractArrayByKey(rooftopObj, "pulse_sources");
        if (!pulseArray.empty())
        {
            next.rooftop.pulseSources.clear();
            for (const auto& obj : SplitTopLevelObjectsInArray(pulseArray))
            {
                SpriteRectConfig fallback{};
                fallback.spritePath = "Asset/Hvac.png";
                next.rooftop.pulseSources.push_back(ParseSpriteRect(obj, fallback));
            }
        }
        const std::string liftBtnObj = ExtractObjectByKey(rooftopObj, "lift_button");
        if (!liftBtnObj.empty())
            next.rooftop.liftButton = ParseSpriteRect(liftBtnObj, next.rooftop.liftButton);
    }

    const std::string undergroundObj = ExtractObjectByKey(text, "underground");
    if (!undergroundObj.empty())
    {
        const std::string obstacleArray = ExtractArrayByKey(undergroundObj, "obstacles");
        if (!obstacleArray.empty())
        {
            next.underground.obstacles.clear();
            for (const auto& obj : SplitTopLevelObjectsInArray(obstacleArray))
                next.underground.obstacles.push_back(ParseSpriteRect(obj, SpriteRectConfig{}));
        }

        const std::string pulseArray = ExtractArrayByKey(undergroundObj, "pulse_sources");
        if (!pulseArray.empty())
        {
            next.underground.pulseSources.clear();
            for (const auto& obj : SplitTopLevelObjectsInArray(pulseArray))
                next.underground.pulseSources.push_back(ParseSpriteRect(obj, SpriteRectConfig{}));
        }

        const std::string rampArray = ExtractArrayByKey(undergroundObj, "ramps");
        if (!rampArray.empty())
        {
            next.underground.ramps.clear();
            for (const auto& obj : SplitTopLevelObjectsInArray(rampArray))
                next.underground.ramps.push_back(ParseSpriteRect(obj, SpriteRectConfig{}));
        }

        const std::string lightsArray = ExtractArrayByKey(undergroundObj, "lights");
        if (!lightsArray.empty())
        {
            next.underground.lights.clear();
            for (const auto& obj : SplitTopLevelObjectsInArray(lightsArray))
                next.underground.lights.push_back(ParseSpriteRect(obj, SpriteRectConfig{}));
        }

        const std::string robotArray = ExtractArrayByKey(undergroundObj, "robot_spawns");
        if (!robotArray.empty())
        {
            next.underground.robotSpawns.clear();
            std::regex arrEntry("\\[[^\\]]*\\]");
            auto begin = std::sregex_iterator(robotArray.begin(), robotArray.end(), arrEntry);
            auto end = std::sregex_iterator();
            for (auto it = begin; it != end; ++it)
                next.underground.robotSpawns.push_back(ParseVec2Direct(it->str(), {}));
        }
    }

    const std::string trainObj = ExtractObjectByKey(text, "train");
    const std::string legacySubwayObj = ExtractObjectByKey(text, "subway");
    const std::string& trainObjRef = !trainObj.empty() ? trainObj : legacySubwayObj;
    if (!trainObjRef.empty())
    {
        const std::string obstacleArray = ExtractArrayByKey(trainObjRef, "obstacles");
        if (!obstacleArray.empty())
        {
            next.train.obstacles.clear();
            for (const auto& obj : SplitTopLevelObjectsInArray(obstacleArray))
                next.train.obstacles.push_back(ParseSpriteRect(obj, SpriteRectConfig{}));
        }

        const std::string pulseArray = ExtractArrayByKey(trainObjRef, "pulse_sources");
        if (!pulseArray.empty())
        {
            next.train.pulseSources.clear();
            for (const auto& obj : SplitTopLevelObjectsInArray(pulseArray))
                next.train.pulseSources.push_back(ParseSpriteRect(obj, SpriteRectConfig{}));
        }
    }

    m_data = next;
    return true;
}

bool MapObjectConfig::Load()
{
    if (!m_didResolveConfigPath)
    {
        m_path = ResolveMapObjectConfigPathString();
        m_didResolveConfigPath = true;
        Logger::Instance().Log(Logger::Severity::Info,
            "MapObjectConfig: config file: %s", m_path.c_str());
    }
    bool ok = ParseFile();
    long long ts = GetFileWriteTime(m_path);
    if (ts > 0)
    {
        m_lastWriteTime = ts;
        m_hasTimestamp = true;
    }
    return ok;
}

bool MapObjectConfig::ReloadIfChanged()
{
    long long current = GetFileWriteTime(m_path);
    if (current <= 0)
    {
        m_hasTimestamp = false;
        return false;
    }

    if (!m_hasTimestamp)
    {
        m_lastWriteTime = current;
        m_hasTimestamp = true;
        return Load();
    }

    if (current == m_lastWriteTime) return false;
    m_lastWriteTime = current;

    const bool ok = ParseFile();
    if (ok)
    {
        Logger::Instance().Log(Logger::Severity::Info,
            "MapObjectConfig: hot-reloaded %s", m_path.c_str());
    }
    return ok;
}

