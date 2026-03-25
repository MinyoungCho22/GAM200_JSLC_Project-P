#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../ThirdParty/json/nlohmann_json.hpp"

class Robot; // Forward declaration

/**
 * @brief Live robot state data for real-time debugging and persistence
 */
struct LiveRobotState
{
    int robotIndex = -1;
    std::string mapName = "Unknown";
    float positionX = 0.0f;
    float positionY = 0.0f;
    float sizeX = 100.0f;
    float sizeY = 100.0f;
    float hp = 100.0f;
    float maxHP = 100.0f;
    float directionX = 1.0f;
    int state = 0; // RobotState as int
    
    // UI editing helpers
    bool modified = false;

    // JSON serialization
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LiveRobotState,
        robotIndex, mapName, positionX, positionY, sizeX, sizeY,
        hp, maxHP, directionX, state)
};

/**
 * @brief Manages robot configurations with JSON file persistence
 */
class RobotConfigManager
{
public:
    RobotConfigManager();
    ~RobotConfigManager();

    /**
     * @brief Save live robot states to JSON file
     * @param filename JSON file path
     * @return true if saved successfully
     */
    bool SaveLiveStatesToFile(const std::string& filename = "live_robot_states.json");

    /**
     * @brief Load live robot states from JSON file
     * @param filename JSON file path
     * @return true if loaded successfully
     */
    bool LoadLiveStatesFromFile(const std::string& filename = "live_robot_states.json");

    /**
     * @brief Get live robot state by map and index
     * @param mapName Map name
     * @param robotIndex Robot index in that map
     * @return Pointer to live state or nullptr if not found
     */
    LiveRobotState* GetLiveState(const std::string& mapName, int robotIndex);

    /**
     * @brief Update or create live robot state
     * @param state Live robot state to update
     */
    void UpdateLiveState(const LiveRobotState& state);

    /**
     * @brief Get all live robot states
     * @return Vector of all live states
     */
    const std::vector<LiveRobotState>& GetAllLiveStates() const { return m_liveStates; }

    /**
     * @brief Apply live state to a robot
     * @param mapName Map name
     * @param robotIndex Robot index
     * @param robot Robot to apply state to
     * @return true if state was applied
     */
    bool ApplyLiveStateToRobot(const std::string& mapName, int robotIndex, class Robot& robot);

    /**
     * @brief Enable/disable auto-save
     */
    void SetAutoSaveEnabled(bool enabled) { m_autoSaveEnabled = enabled; }

    /**
     * @brief Auto-save modified states
     */
    void AutoSave();

private:
    std::vector<LiveRobotState> m_liveStates;
    std::string m_liveStatesFilename;
    bool m_autoSaveEnabled = true;
};
