#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../ThirdParty/json/nlohmann_json.hpp"

class Drone; // Forward declaration

/**
 * @brief Drone configuration data structure
 */
struct DroneConfigData
{
    std::string name = "Default Drone";
    float baseSpeed = 200.0f;
    float detectionRange = 150.0f;
    float timeToDestroy = 1.0f;
    float acceleration = 800.0f;
    float attackRadius = 100.0f;
    float attackAngle = 0.0f;
    int attackDirection = 1;
    float radarRotationSpeed = 200.0f;
    float radarLength = 150.0f;
    bool isTracer = false;

    // UI editing helpers
    bool modified = false;

    // JSON serialization
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(DroneConfigData,
        name, baseSpeed, detectionRange, timeToDestroy, acceleration,
        attackRadius, attackAngle, attackDirection, radarRotationSpeed,
        radarLength, isTracer)
};

/**
 * @brief Live drone state data for real-time debugging
 */
struct LiveDroneState
{
    int droneIndex = -1;
    std::string mapName = "Unknown";
    float positionX = 0.0f;
    float positionY = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float sizeX = 64.0f;
    float sizeY = 64.0f;
    float baseSpeed = 200.0f;
    float attackRadius = 100.0f;
    float attackAngle = 0.0f;
    int attackDirection = 1;
    float damageTimer = 0.0f;
    float maxHP = 100.0f;  // Only save maxHP, current HP resets to max on game start
    bool isDebugMode = false;
    bool isDead = false;
    bool isHit = false;
    bool isAttacking = false;

    // UI editing helpers
    bool modified = false;

    // JSON serialization
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LiveDroneState,
        droneIndex, mapName, positionX, positionY, velocityX, velocityY,
        sizeX, sizeY, baseSpeed, attackRadius, attackAngle, attackDirection,
        damageTimer, maxHP, isDebugMode, isDead, isHit, isAttacking)
};

/**
 * @brief Manages drone configurations with JSON file persistence
 */
class DroneConfigManager
{
public:
    DroneConfigManager();
    ~DroneConfigManager();

    /**
     * @brief Load configurations from JSON file
     * @param filename JSON file path
     * @return true if loaded successfully
     */
    bool LoadFromFile(const std::string& filename = "drone_config.json");

    /**
     * @brief Save configurations to JSON file
     * @param filename JSON file path
     * @return true if saved successfully
     */
    bool SaveToFile(const std::string& filename = "drone_config.json");

    /**
     * @brief Get configuration by index
     * @param index Configuration index
     * @return Pointer to config data or nullptr if invalid index
     */
    DroneConfigData* GetConfig(size_t index);

    /**
     * @brief Get configuration by name
     * @param name Configuration name
     * @return Pointer to config data or nullptr if not found
     */
    DroneConfigData* GetConfigByName(const std::string& name);

    /**
     * @brief Add new configuration
     * @param config Configuration to add
     * @return Index of added configuration
     */
    size_t AddConfig(const DroneConfigData& config);

    /**
     * @brief Remove configuration by index
     * @param index Configuration index to remove
     */
    void RemoveConfig(size_t index);

    /**
     * @brief Get all configurations
     * @return Vector of all configurations
     */
    const std::vector<DroneConfigData>& GetAllConfigs() const { return m_configs; }

    /**
     * @brief Get number of configurations
     * @return Number of configurations
     */
    size_t GetConfigCount() const { return m_configs.size(); }

    /**
     * @brief Mark configuration as modified (will be auto-saved)
     * @param index Configuration index
     */
    void MarkModified(size_t index);

    /**
     * @brief Auto-save modified configurations
     */
    void AutoSave();

    /**
     * @brief Save live drone states to JSON file
     * @param filename JSON file path
     * @return true if saved successfully
     */
    bool SaveLiveStatesToFile(const std::string& filename = "live_drone_states.json");

    /**
     * @brief Load live drone states from JSON file
     * @param filename JSON file path
     * @return true if loaded successfully
     */
    bool LoadLiveStatesFromFile(const std::string& filename = "live_drone_states.json");

    /**
     * @brief Get live drone state by map and index
     * @param mapName Map name
     * @param droneIndex Drone index in that map
     * @return Pointer to live state or nullptr if not found
     */
    LiveDroneState* GetLiveState(const std::string& mapName, int droneIndex);

    /**
     * @brief Update or create live drone state
     * @param state Live drone state to update
     */
    void UpdateLiveState(const LiveDroneState& state);

    /**
     * @brief Get all live drone states
     * @return Vector of all live states
     */
    const std::vector<LiveDroneState>& GetAllLiveStates() const { return m_liveStates; }

    /**
     * @brief Apply live state to a drone
     * @param mapName Map name
     * @param droneIndex Drone index
     * @param drone Drone to apply state to
     * @return true if state was applied
     */
    bool ApplyLiveStateToDrone(const std::string& mapName, int droneIndex, class Drone& drone);

private:
    std::vector<DroneConfigData> m_configs;
    std::string m_filename;
    bool m_autoSaveEnabled = true;

    std::vector<LiveDroneState> m_liveStates;
    std::string m_liveStatesFilename;
};