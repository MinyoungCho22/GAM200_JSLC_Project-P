#include "RobotConfig.hpp"
#include "Logger.hpp"
#include "../Game/Robot.hpp"
#include "../Engine/Vec2.hpp"

#include <filesystem>
#include <fstream>
#include <sys/stat.h>

namespace fs = std::filesystem;
#ifdef _WIN32
    #include <stdlib.h> // for _fullpath
#else
    #include <limits.h> // for PATH_MAX
    #include <stdlib.h> // for realpath
#endif

RobotConfigManager::RobotConfigManager() = default;

RobotConfigManager::~RobotConfigManager()
{
    // Auto-save on destruction
    SaveLiveStatesToFile();
}

bool RobotConfigManager::SaveLiveStatesToFile(const std::string& filename)
{
    if (!filename.empty())
        m_liveStatesFilename = filename;
    if (m_liveStatesFilename.empty())
        m_liveStatesFilename = "Config/live_robot_states.json";

    try
    {
        fs::path outPath(m_liveStatesFilename);
        if (outPath.has_parent_path())
            fs::create_directories(outPath.parent_path());

        nlohmann::json j = m_liveStates;
        
        // Get absolute path for logging
        char absolutePath[1024];
        #ifdef _WIN32
            _fullpath(absolutePath, m_liveStatesFilename.c_str(), sizeof(absolutePath));
        #else
            realpath(m_liveStatesFilename.c_str(), absolutePath);
        #endif
        
        std::ofstream file(m_liveStatesFilename);
        if (!file.is_open())
        {
            Logger::Instance().Log(Logger::Severity::Error,
                "RobotConfig: Failed to open file for writing '%s' (absolute: %s)", 
                m_liveStatesFilename.c_str(), absolutePath);
            return false;
        }
        file << j.dump(4); // Pretty print with 4 spaces
        file.close();

        // Reset modified flags
        for (auto& state : m_liveStates)
        {
            state.modified = false;
        }

        Logger::Instance().Log(Logger::Severity::Info,
            "RobotConfig: Saved %zu robot states to '%s' (absolute: %s)", 
            m_liveStates.size(), m_liveStatesFilename.c_str(), absolutePath);
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::Instance().Log(Logger::Severity::Error,
            "RobotConfig: Failed to save file '%s': %s", m_liveStatesFilename.c_str(), e.what());
        return false;
    }
}

bool RobotConfigManager::LoadLiveStatesFromFile(const std::string& filename)
{
    std::vector<std::string> candidates;
    if (!filename.empty())
        candidates.push_back(filename);
    else
    {
        candidates.push_back("Config/live_robot_states.json");
        candidates.push_back("live_robot_states.json");
    }

    for (const std::string& path : candidates)
    {
        try
        {
            struct stat buffer;
            if (stat(path.c_str(), &buffer) != 0)
                continue;

            std::ifstream file(path);
            if (!file.is_open())
            {
                Logger::Instance().Log(Logger::Severity::Error,
                    "RobotConfig: Failed to open file '%s'", path.c_str());
                continue;
            }

            nlohmann::json j;
            file >> j;
            file.close();

            m_liveStates = j.get<std::vector<LiveRobotState>>();
            m_liveStatesFilename = path;

            for (auto& state : m_liveStates)
                state.modified = false;

            Logger::Instance().Log(Logger::Severity::Info,
                "RobotConfig: Loaded %zu robot states from '%s'", m_liveStates.size(), path.c_str());
            return true;
        }
        catch (const std::exception& e)
        {
            Logger::Instance().Log(Logger::Severity::Error,
                "RobotConfig: Failed to load file '%s': %s", path.c_str(), e.what());
        }
    }

    if (filename.empty())
    {
        Logger::Instance().Log(Logger::Severity::Debug,
            "RobotConfig: No live robot state file found (Config/ or legacy path)");
    }
    return false;
}

LiveRobotState* RobotConfigManager::GetLiveState(const std::string& mapName, int robotIndex)
{
    for (auto& state : m_liveStates)
    {
        if (state.mapName == mapName && state.robotIndex == robotIndex)
        {
            return &state;
        }
    }
    return nullptr;
}

void RobotConfigManager::UpdateLiveState(const LiveRobotState& state)
{
    // Find existing state
    for (auto& s : m_liveStates)
    {
        if (s.mapName == state.mapName && s.robotIndex == state.robotIndex)
        {
            s = state;
            s.modified = true;
            
            if (m_autoSaveEnabled)
            {
                AutoSave();
            }
            return;
        }
    }

    // If not found, add new state
    m_liveStates.push_back(state);
    m_liveStates.back().modified = true;

    if (m_autoSaveEnabled)
    {
        AutoSave();
    }
}

bool RobotConfigManager::ApplyLiveStateToRobot(const std::string& mapName, int robotIndex, Robot& robot)
{
    LiveRobotState* state = GetLiveState(mapName, robotIndex);
    if (!state)
        return false;

    robot.SetPosition({ state->positionX, state->positionY });
    robot.SetSize({ state->sizeX, state->sizeY });
    robot.SetHP(state->hp);
    robot.SetMaxHP(state->maxHP);
    robot.SetDirectionX(state->directionX);
    robot.SetState(static_cast<RobotState>(state->state));

    Logger::Instance().Log(Logger::Severity::Debug,
        "RobotConfig: Applied state to robot %d in map '%s'", robotIndex, mapName.c_str());
    return true;
}

void RobotConfigManager::AutoSave()
{
    // Check if any state is modified
    bool hasModified = false;
    for (const auto& state : m_liveStates)
    {
        if (state.modified)
        {
            hasModified = true;
            break;
        }
    }

    if (hasModified)
    {
        SaveLiveStatesToFile();
    }
}
