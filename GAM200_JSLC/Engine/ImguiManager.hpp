#pragma once

#include "DroneConfig.hpp"
#include <vector>
#include <memory>

struct GLFWwindow;
class DroneManager;
class Drone;

/**
 * @brief Dear ImGui debug window manager (GLFW + OpenGL3).
 *
 * Creates a separate debug window for real-time game debugging.
 * - If Dear ImGui (and its GLFW/OpenGL3 backends) are not available in the build,
 *   this manager becomes a no-op and the engine will continue to run.
 */
class ImguiManager
{
public:
    bool Initialize();
    void Shutdown();

    // Call once per frame (typically after your game update) to render the debug window.
    void BeginFrame(double dt);
    void DrawDebugWindow();
    void EndFrame();

    void SetWarningLevel(int level);
    void SetDroneManager(DroneManager* manager) { m_droneManager = manager; }
    void SetDroneConfigManager(std::shared_ptr<DroneConfigManager> configManager) { m_configManager = configManager; }
    void AddMapDroneManager(const std::string& mapName, DroneManager* manager);

           void SetEnabled(bool enabled) { m_enabled = enabled; }
           bool IsEnabled() const { return m_enabled; }
           bool IsInitialized() const { return m_initialized; }

           // Player god mode access
           bool IsPlayerGodMode() const { return m_playerGodMode; }

    int GetAverageFps() const { return m_averageFps; }
    int GetWarningLevel() const { return m_warningLevel; }

private:
    void UpdateFps(double dt);
    void DrawDroneDebugPanel();
    void DrawLiveDronePanel();
    void DrawDroneList(DroneManager* manager);
    void DrawDroneProperties(Drone& drone, int index);
    void DrawDroneConfigPanel();
    void DrawDroneConfigProperties(int configIndex);
    void ApplyConfigToDrone(const DroneConfigData& config, Drone& drone);

    GLFWwindow* m_debugWindow = nullptr;
    GLFWwindow* m_mainWindow = nullptr; // Save main window context
    bool m_initialized = false;
    bool m_enabled = true;

    double m_fpsTimer = 0.0;
    int m_frameCount = 0;
    int m_averageFps = 0;

    int m_warningLevel = 0;
    bool m_hasWarningLevel = false;

    DroneManager* m_droneManager = nullptr;
    std::shared_ptr<DroneConfigManager> m_configManager;
    std::vector<std::pair<std::string, DroneManager*>> m_mapDroneManagers;
    int m_selectedDroneIndex = -1;
    int m_selectedConfigIndex = -1;
    bool m_droneDebugMode = false; // When true, selected drone is in debug mode
    int m_selectedMapIndex = 0; // Which map's drones to show

    // Live drone state management
    std::string m_selectedMapName = "Hallway";
    void SaveDroneState(Drone& drone, int index, const std::string& mapName);
    void LoadDroneState(Drone& drone, int index, const std::string& mapName);

    // Player god mode (infinite pulse)
    bool m_playerGodMode = false;
};

