#include "DroneConfig.hpp"
#include "../Game/Drone.hpp"
#include "../Engine/Vec2.hpp"
#include "Logger.hpp"
#include <filesystem>
#include <fstream>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace
{
long long GetFileWriteStamp(const std::string& path)
{
    struct stat st {};
    if (stat(path.c_str(), &st) != 0)
        return 0;
    return (static_cast<long long>(st.st_mtime) << 32) ^ static_cast<long long>(st.st_size);
}
}

DroneConfigManager::DroneConfigManager()
{
    if (!LoadFromFile("Config/drone_config.json") && !LoadFromFile("drone_config.json"))
    {
        m_filename = "Config/drone_config.json";
        DroneConfigData defaultConfig;
        defaultConfig.name = "Default Drone";
        AddConfig(defaultConfig);

        DroneConfigData tracerConfig;
        tracerConfig.name = "Tracer Drone";
        tracerConfig.isTracer = true;
        tracerConfig.baseSpeed = 250.0f;
        tracerConfig.detectionRange = 200.0f;
        AddConfig(tracerConfig);

        SaveToFile();
    }
}

DroneConfigManager::~DroneConfigManager()
{
    // Auto-save on destruction
    SaveToFile();
}

bool DroneConfigManager::LoadFromFile(const std::string& filename)
{
    m_filename = filename;

    try
    {
        // Check if file exists using traditional method
        struct stat buffer;
        if (stat(filename.c_str(), &buffer) != 0)
        {
            Logger::Instance().Log(Logger::Severity::Debug,
                "DroneConfig: Config file '%s' does not exist, will create defaults", filename.c_str());
            return false;
        }

        std::ifstream file(filename);
        if (!file.is_open())
        {
            Logger::Instance().Log(Logger::Severity::Error,
                "DroneConfig: Failed to open config file '%s'", filename.c_str());
            return false;
        }

        nlohmann::json jsonData;
        file >> jsonData;

        m_configs.clear();
        for (const auto& item : jsonData)
        {
            DroneConfigData config = item.get<DroneConfigData>();
            config.modified = false; // Reset modified flag on load
            m_configs.push_back(config);
        }

        Logger::Instance().Log(Logger::Severity::Info,
            "DroneConfig: Loaded %zu configurations from '%s'", m_configs.size(), filename.c_str());
        m_configWriteTime = GetFileWriteStamp(filename);
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::Instance().Log(Logger::Severity::Error,
            "DroneConfig: Failed to load config file '%s': %s", filename.c_str(), e.what());
        return false;
    }
}

bool DroneConfigManager::SaveToFile(const std::string& filename)
{
    if (!filename.empty() && filename != m_filename)
        m_filename = filename;
    if (m_filename.empty())
        m_filename = "Config/drone_config.json";

    try
    {
        fs::path outPath(m_filename);
        if (outPath.has_parent_path())
            fs::create_directories(outPath.parent_path());

        nlohmann::json jsonData = m_configs;

        std::ofstream file(m_filename);
        if (!file.is_open())
        {
            Logger::Instance().Log(Logger::Severity::Error,
                "DroneConfig: Failed to open config file for writing '%s'", filename.c_str());
            return false;
        }

        file << jsonData.dump(4); // Pretty print with 4 spaces
        file.close();
        m_configWriteTime = GetFileWriteStamp(m_filename);

        // Reset modified flags
        for (auto& config : m_configs)
        {
            config.modified = false;
        }

        Logger::Instance().Log(Logger::Severity::Info,
            "DroneConfig: Saved %zu configurations to '%s'", m_configs.size(), filename.c_str());
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::Instance().Log(Logger::Severity::Error,
            "DroneConfig: Failed to save config file '%s': %s", filename.c_str(), e.what());
        return false;
    }
}

DroneConfigData* DroneConfigManager::GetConfig(size_t index)
{
    if (index >= m_configs.size())
        return nullptr;
    return &m_configs[index];
}

DroneConfigData* DroneConfigManager::GetConfigByName(const std::string& name)
{
    for (auto& config : m_configs)
    {
        if (config.name == name)
            return &config;
    }
    return nullptr;
}

size_t DroneConfigManager::AddConfig(const DroneConfigData& config)
{
    m_configs.push_back(config);
    m_configs.back().modified = true;
    Logger::Instance().Log(Logger::Severity::Info,
        "DroneConfig: Added new configuration '%s'", config.name.c_str());
    return m_configs.size() - 1;
}

void DroneConfigManager::RemoveConfig(size_t index)
{
    if (index >= m_configs.size())
        return;

    Logger::Instance().Log(Logger::Severity::Info,
        "DroneConfig: Removed configuration '%s'", m_configs[index].name.c_str());
    m_configs.erase(m_configs.begin() + index);
}

void DroneConfigManager::MarkModified(size_t index)
{
    if (index >= m_configs.size())
        return;

    m_configs[index].modified = true;

    // Auto-save if enabled
    if (m_autoSaveEnabled)
    {
        AutoSave();
    }
}

void DroneConfigManager::AutoSave()
{
    // Check if any config is modified
    bool hasModified = false;
    for (const auto& config : m_configs)
    {
        if (config.modified)
        {
            hasModified = true;
            break;
        }
    }

    if (hasModified)
    {
        SaveToFile();
    }
}

bool DroneConfigManager::SaveLiveStatesToFile(const std::string& filename)
{
    try
    {
        std::string path = filename.empty() ? m_liveStatesFilename : filename;
        if (path.empty())
            path = "Config/live_drone_states.json";

        fs::path outPath(path);
        if (outPath.has_parent_path())
            fs::create_directories(outPath.parent_path());

        nlohmann::json j = m_liveStates;
        std::ofstream file(path);
        if (!file.is_open())
        {
            return false;
        }
        file << j.dump(4); // Pretty print with 4 spaces
        file.close();
        m_liveStatesFilename = path;
        m_liveStatesWriteTime = GetFileWriteStamp(path);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool DroneConfigManager::LoadLiveStatesFromFile(const std::string& filename)
{
    auto tryLoad = [this](const std::string& path) -> bool {
        try
        {
            std::ifstream file(path);
            if (!file.is_open())
                return false;

            nlohmann::json j;
            file >> j;
            file.close();

            m_liveStates = j.get<std::vector<LiveDroneState>>();
            m_liveStatesFilename = path;
            m_liveStatesWriteTime = GetFileWriteStamp(path);
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    };

    if (!filename.empty())
        return tryLoad(filename);

    if (tryLoad("Config/live_drone_states.json"))
        return true;
    return tryLoad("live_drone_states.json");
}

bool DroneConfigManager::ReloadConfigIfChanged()
{
    if (m_filename.empty())
        m_filename = "Config/drone_config.json";

    const long long current = GetFileWriteStamp(m_filename);
    if (current <= 0 || current == m_configWriteTime)
        return false;

    return LoadFromFile(m_filename);
}

bool DroneConfigManager::ReloadLiveStatesIfChanged()
{
    if (m_liveStatesFilename.empty())
        m_liveStatesFilename = "Config/live_drone_states.json";

    const long long current = GetFileWriteStamp(m_liveStatesFilename);
    if (current <= 0 || current == m_liveStatesWriteTime)
        return false;

    return LoadLiveStatesFromFile(m_liveStatesFilename);
}

LiveDroneState* DroneConfigManager::GetLiveState(const std::string& mapName, int droneIndex)
{
    for (auto& state : m_liveStates)
    {
        if (state.mapName == mapName && state.droneIndex == droneIndex)
        {
            return &state;
        }
    }
    return nullptr;
}

void DroneConfigManager::UpdateLiveState(const LiveDroneState& state)
{
    for (auto& existingState : m_liveStates)
    {
        if (existingState.mapName == state.mapName && existingState.droneIndex == state.droneIndex)
        {
            existingState = state;
            existingState.modified = true;
            SaveLiveStatesToFile();
            return;
        }
    }

    // New state
    LiveDroneState newState = state;
    newState.modified = true;
    m_liveStates.push_back(newState);
    SaveLiveStatesToFile();
}

bool DroneConfigManager::ApplyLiveStateToDrone(const std::string& mapName, int droneIndex, Drone& drone)
{
    LiveDroneState* state = GetLiveState(mapName, droneIndex);
    if (!state)
    {
        return false;
    }

    // Apply state to drone
    drone.SetPosition(Math::Vec2(state->positionX, state->positionY));
    drone.SetVelocity(Math::Vec2(state->velocityX, state->velocityY));
    drone.SetSize(Math::Vec2(state->sizeX, state->sizeY));
    drone.SetBaseSpeedDebug(state->baseSpeed);
    drone.SetAttackRadius(state->attackRadius);
    drone.SetAttackAngle(state->attackAngle);
    drone.SetAttackDirection(state->attackDirection);
    drone.SetDamageTimer(state->damageTimer);
    drone.SetMaxHP(state->maxHP);
    drone.SetHP(state->maxHP);  // Reset HP to max on load
    drone.SetDebugMode(state->isDebugMode);

    if (state->isDead) drone.SetDead(true);
    if (state->isHit) drone.SetHit(true);
    if (state->isAttacking) drone.SetAttacking(true);

    return true;
}