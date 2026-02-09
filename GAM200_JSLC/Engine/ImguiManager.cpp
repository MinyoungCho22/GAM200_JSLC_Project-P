// ImguiManager.cpp

#include "ImguiManager.hpp"
#include "Logger.hpp"

#include "../include/GLFW/glfw3.h"
#include "../Game/DroneManager.hpp"
#include "../Game/Drone.hpp"
#include "../Game/Robot.hpp"
#include "../Game/Underground.hpp"
#include "../Engine/Vec2.hpp"

#include "../ThirdParty/imgui/imgui.h"
#include "../ThirdParty/imgui/backends/imgui_impl_glfw.h"
#include "../ThirdParty/imgui/backends/imgui_impl_opengl3.h"


bool ImguiManager::Initialize()
{
    // Save the main window (current context)
    m_mainWindow = glfwGetCurrentContext();
    if (!m_mainWindow)
    {
        Logger::Instance().Log(Logger::Severity::Error, "ImguiManager: No main window context found.");
        m_initialized = false;
        return false;
    }

    // Create a separate debug window for ImGui
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);

    m_debugWindow = glfwCreateWindow(800, 600, "Game Debug Window", nullptr, nullptr);
    if (!m_debugWindow)
    {
        Logger::Instance().Log(Logger::Severity::Error, "ImguiManager: Failed to create debug window.");
        m_initialized = false;
        return false;
    }

    // Position the debug window to the right of the main window
    int mainX, mainY, mainW, mainH;
    glfwGetWindowPos(m_mainWindow, &mainX, &mainY);
    glfwGetWindowSize(m_mainWindow, &mainW, &mainH);
    glfwSetWindowPos(m_debugWindow, mainX + mainW + 10, mainY);

    glfwMakeContextCurrent(m_debugWindow);
    glfwSwapInterval(1); // Enable vsync for debug window

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    ImGui::StyleColorsDark();

    // For OpenGL 3.3 core profile in this project.
    const char* glsl_version = "#version 330";

    if (!ImGui_ImplGlfw_InitForOpenGL(m_debugWindow, true))
    {
        Logger::Instance().Log(Logger::Severity::Error, "ImguiManager: ImGui_ImplGlfw_InitForOpenGL failed.");
        glfwDestroyWindow(m_debugWindow);
        ImGui::DestroyContext();
        m_initialized = false;
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init(glsl_version))
    {
        Logger::Instance().Log(Logger::Severity::Error, "ImguiManager: ImGui_ImplOpenGL3_Init failed.");
        ImGui_ImplGlfw_Shutdown();
        glfwDestroyWindow(m_debugWindow);
        ImGui::DestroyContext();
        m_initialized = false;
        return false;
    }

    m_initialized = true;
    Logger::Instance().Log(Logger::Severity::Info, "ImguiManager: Debug window initialized.");
    return true;
}

void ImguiManager::Shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_debugWindow)
    {
        glfwDestroyWindow(m_debugWindow);
        m_debugWindow = nullptr;
    }

    m_initialized = false;
}

void ImguiManager::SetWarningLevel(int level)
{
    m_warningLevel = level;
    m_hasWarningLevel = true;
}

void ImguiManager::UpdateFps(double dt)
{
    m_fpsTimer += dt;
    m_frameCount++;

    if (m_fpsTimer >= 1.0)
    {
        m_averageFps = (m_fpsTimer > 0.0) ? static_cast<int>(m_frameCount / m_fpsTimer) : 0;
        m_fpsTimer -= 1.0;
        m_frameCount = 0;
    }
}

void ImguiManager::BeginFrame(double dt)
{
    if (!m_initialized || !m_enabled)
    {
        return;
    }

    UpdateFps(dt);

    // Note: ImGui frame setup is now handled in DrawDebugWindow()
}

void ImguiManager::DrawDebugWindow()
{
    if (!m_initialized || !m_enabled)
    {
        return;
    }

    // Switch to debug window context
    glfwMakeContextCurrent(m_debugWindow);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Main menu bar
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit")) {
                glfwSetWindowShouldClose(m_debugWindow, true);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            // Demo window removed for simplicity
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // Performance window
    ImGui::Begin("Performance");
    ImGui::Text("FPS: %d", m_averageFps);
    ImGui::Text("Frame Time: %.3f ms", 1000.0 / m_averageFps);

    if (m_hasWarningLevel)
    {
        ImVec4 color{ 0.8f, 0.8f, 0.8f, 1.0f };
        if (m_warningLevel >= 2) color = ImVec4{ 1.0f, 0.25f, 0.25f, 1.0f };
        else if (m_warningLevel == 1) color = ImVec4{ 1.0f, 0.85f, 0.25f, 1.0f };
        else color = ImVec4{ 0.45f, 1.0f, 0.45f, 1.0f };

        ImGui::TextColored(color, "Warning Level: %d", m_warningLevel);
    }

    ImGui::Separator();
    ImGui::Text("Player Settings:");

    // Player God Mode (Infinite Pulse)
    ImGui::Checkbox("God Mode (Infinite Pulse)", &m_playerGodMode);
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("When enabled:");
        ImGui::Text("- Pulse never depletes");
        ImGui::Text("- Can use abilities infinitely");
        ImGui::Text("- Perfect for testing/debugging");
        ImGui::EndTooltip();
    }

    ImGui::End();

    // Drone debug panel (includes robots for Underground map)
    DrawDroneDebugPanel();

    // Demo window removed for simplicity

    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(m_debugWindow, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(m_debugWindow);

    // Switch back to main window context
    glfwMakeContextCurrent(m_mainWindow);
}

void ImguiManager::DrawDroneDebugPanel()
{
    ImGui::Begin("Drone Debug");

    if (ImGui::BeginTabBar("DebugTabs"))
    {
        // Live Drones tab
        if (ImGui::BeginTabItem("Live Drones"))
        {
            DrawLiveDronePanel();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void ImguiManager::DrawLiveDronePanel()
{
    // Map selection combo box
    if (!m_mapDroneManagers.empty())
    {
        std::vector<const char*> mapNames;
        for (const auto& pair : m_mapDroneManagers)
        {
            mapNames.push_back(pair.first.c_str());
        }

        if (ImGui::Combo("Map", &m_selectedMapIndex, mapNames.data(), static_cast<int>(mapNames.size())))
        {
            m_selectedDroneIndex = -1; // Reset drone selection when changing maps
            m_selectedRobotIndex = -1; // Reset robot selection when changing maps
        }
    }

    // Get drones from selected map
    DroneManager* currentManager = nullptr;
    std::string currentMapName = "None";

    if (m_selectedMapIndex >= 0 && m_selectedMapIndex < static_cast<int>(m_mapDroneManagers.size()))
    {
        currentManager = m_mapDroneManagers[m_selectedMapIndex].second;
        currentMapName = m_mapDroneManagers[m_selectedMapIndex].first;
        m_selectedMapName = currentMapName; // Update selected map name for save/load
    }
    else if (m_droneManager)
    {
        currentManager = m_droneManager;
        currentMapName = "Main";
        m_selectedMapName = currentMapName; // Update selected map name for save/load
    }

    if (!currentManager)
    {
        ImGui::Text("No drone manager available");
        return;
    }

    const auto& drones = currentManager->GetDrones();
    
    // Check if current map is Underground to show robots
    bool isUnderground = (currentMapName == "Underground");
    size_t robotCount = 0;
    if (isUnderground && m_underground)
    {
        robotCount = m_underground->GetRobots().size();
    }
    
    if (isUnderground)
    {
        ImGui::Text("Map: %s | Drones: %zu | Robots: %zu", currentMapName.c_str(), drones.size(), robotCount);
    }
    else
    {
        ImGui::Text("Map: %s | Total Drones: %zu", currentMapName.c_str(), drones.size());
    }

    if (ImGui::BeginTabBar("LiveDroneTabs"))
    {
        // Drones tab with list and properties (like robots)
        if (ImGui::BeginTabItem("Drones"))
        {
            if (drones.empty())
            {
                ImGui::Text("No drones in this map");
            }
            else
            {
                ImGui::Text("Total Drones: %zu", drones.size());
                ImGui::Separator();

                // List of drones
                for (size_t i = 0; i < drones.size(); ++i)
                {
                    const Drone& drone = drones[i];
                    
                    ImGui::PushID(static_cast<int>(i));
                    
                    std::string label = "Drone #" + std::to_string(i);
                    if (drone.IsDead())
                    {
                        label += " [DEAD]";
                    }
                    else if (drone.IsDebugMode())
                    {
                        label += " [DEBUG]";
                    }
                    
                    if (ImGui::Selectable(label.c_str(), m_selectedDroneIndex == static_cast<int>(i)))
                    {
                        m_selectedDroneIndex = static_cast<int>(i);
                    }
                    
                    ImGui::PopID();
                }

                ImGui::Separator();

                // Properties of selected drone
                if (m_selectedDroneIndex >= 0 && m_selectedDroneIndex < static_cast<int>(drones.size()))
                {
                    DrawDroneProperties(const_cast<Drone&>(drones[m_selectedDroneIndex]), m_selectedDroneIndex);
                }
            }
            
            ImGui::EndTabItem();
        }

        // Robots tab (only for Underground map)
        if (isUnderground && m_underground)
        {
            if (ImGui::BeginTabItem("Robots"))
            {
                auto& robots = m_underground->GetRobots();
                
                if (robots.empty())
                {
                    ImGui::Text("No robots in this map");
                }
                else
                {
                    ImGui::Text("Total Robots: %zu", robots.size());
                    ImGui::Separator();

                    // List of robots
                    for (size_t i = 0; i < robots.size(); ++i)
                    {
                        Robot& robot = robots[i];
                        
                        ImGui::PushID(static_cast<int>(i + 10000)); // Offset to avoid ID collision with drones
                        
                        std::string label = "Robot #" + std::to_string(i);
                        if (robot.IsDead())
                        {
                            label += " [DEAD]";
                        }
                        
                        if (ImGui::Selectable(label.c_str(), m_selectedRobotIndex == static_cast<int>(i)))
                        {
                            m_selectedRobotIndex = static_cast<int>(i);
                        }
                        
                        ImGui::PopID();
                    }

                    ImGui::Separator();

                    // Properties of selected robot
                    if (m_selectedRobotIndex >= 0 && m_selectedRobotIndex < static_cast<int>(robots.size()))
                    {
                        DrawRobotProperties(robots[m_selectedRobotIndex], m_selectedRobotIndex);
                    }
                }
                
                ImGui::EndTabItem();
            }
        }

        ImGui::EndTabBar();
    }
}

void ImguiManager::DrawDroneList(DroneManager* manager)
{
    if (!manager) return;

    const auto& drones = manager->GetDrones();

    if (ImGui::BeginTable("DroneTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Index");
        ImGui::TableSetupColumn("Position");
        ImGui::TableSetupColumn("Velocity");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Actions");
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(drones.size()); ++i)
        {
            const auto& drone = drones[i];
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", i);

            ImGui::TableSetColumnIndex(1);
            auto pos = drone.GetPosition();
            ImGui::Text("(%.1f, %.1f)", pos.x, pos.y);

            ImGui::TableSetColumnIndex(2);
            auto vel = drone.GetVelocity();
            ImGui::Text("(%.1f, %.1f)", vel.x, vel.y);

            ImGui::TableSetColumnIndex(3);
            std::string status;
            if (drone.IsDead()) status = "Dead";
            else if (drone.IsAttacking()) status = "Attacking";
            else if (drone.IsHit()) status = "Hit";
            else status = "Active";
            ImGui::Text("%s", status.c_str());

            ImGui::TableSetColumnIndex(4);
            if (ImGui::Button(("Select##" + std::to_string(i)).c_str()))
            {
                m_selectedDroneIndex = i;
                // Load saved state for this drone
                LoadDroneState(const_cast<Drone&>(drone), i, m_selectedMapName);
                // Auto-enable debug mode when selecting a drone
                const_cast<Drone&>(drone).SetDebugMode(true);
            }
            ImGui::SameLine();
            if (ImGui::Button(("Kill##" + std::to_string(i)).c_str()))
            {
                // Mark drone as dead for debugging
                const_cast<Drone&>(drone).SetDead(true);
            }
        }

        ImGui::EndTable();
    }
}

void ImguiManager::DrawDroneProperties(Drone& drone, int index)
{
    ImGui::Text("Drone %d Properties", index);

    // Current status display
    ImGui::SameLine(ImGui::GetWindowWidth() - 200);
    if (drone.IsDebugMode())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "DEBUG MODE");
    }
    else
    {
        // Check if AI is delayed after debug mode exit
        float debugExitTimer = 0.0f; // We can't access this directly, so we'll show AI ACTIVE for now
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "AI ACTIVE");
    }

    ImGui::Separator();

    // Debug Mode Toggle
    bool debugMode = drone.IsDebugMode();
    if (ImGui::Checkbox("Debug Mode (FREEZE AI - Manual Control Only)", &debugMode))
    {
        drone.SetDebugMode(debugMode);
        SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("When enabled:");
        ImGui::Text("- AI is COMPLETELY disabled");
        ImGui::Text("- Position and velocity are frozen");
        ImGui::Text("- Only manual controls work");
        ImGui::Text("- Changes are auto-saved");
        ImGui::EndTooltip();
    }

    // Visual indicator when debug mode is active
    if (debugMode)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[FROZEN]");
    }

    ImGui::Separator();

    // Position with +,- buttons
    Math::Vec2 pos = drone.GetPosition();

    ImGui::Text("Position: (%.1f, %.1f)", pos.x, pos.y);

    // X coordinate controls
    ImGui::PushID("pos_x");
    if (ImGui::Button("-10")) pos.x -= 10.0f;
    ImGui::SameLine();
    if (ImGui::Button("-1")) pos.x -= 1.0f;
    ImGui::SameLine();
    ImGui::Text("X: %.1f", pos.x);
    ImGui::SameLine();
    if (ImGui::Button("+1")) pos.x += 1.0f;
    ImGui::SameLine();
    if (ImGui::Button("+10")) pos.x += 10.0f;
    ImGui::PopID();

    // Y coordinate controls
    ImGui::PushID("pos_y");
    if (ImGui::Button("-10")) pos.y -= 10.0f;
    ImGui::SameLine();
    if (ImGui::Button("-1")) pos.y -= 1.0f;
    ImGui::SameLine();
    ImGui::Text("Y: %.1f", pos.y);
    ImGui::SameLine();
    if (ImGui::Button("+1")) pos.y += 1.0f;
    ImGui::SameLine();
    if (ImGui::Button("+10")) pos.y += 10.0f;
    ImGui::PopID();

    // Apply position changes automatically
    Math::Vec2 currentPos = drone.GetPosition();
    bool positionChanged = (pos.x != currentPos.x) || (pos.y != currentPos.y);
    if (positionChanged)
    {
        drone.SetPosition(pos);
        drone.SetVelocity({0.0f, 0.0f});  // Stop movement when position is changed
        drone.SetDebugMode(true);  // Enable debug mode to freeze AI
        SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName);
    }

    // Drag control
    if (ImGui::DragFloat2("Drag Position", &pos.x, 1.0f, -10000.0f, 30000.0f))
    {
        drone.SetPosition(pos);
        drone.SetVelocity({0.0f, 0.0f});  // Stop movement when position is changed
        drone.SetDebugMode(true);  // Enable debug mode to freeze AI
        SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName);
    }

    ImGui::Separator();

    // Velocity with +,- buttons
    Math::Vec2 vel = drone.GetVelocity();
    ImGui::Text("Velocity: (%.1f, %.1f)", vel.x, vel.y);

    // X velocity controls
    ImGui::PushID("vel_x");
    if (ImGui::Button("-10")) { vel.x -= 10.0f; drone.SetVelocity(vel); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    if (ImGui::Button("-1")) { vel.x -= 1.0f; drone.SetVelocity(vel); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    ImGui::Text("VX: %.1f", vel.x);
    ImGui::SameLine();
    if (ImGui::Button("+1")) { vel.x += 1.0f; drone.SetVelocity(vel); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    if (ImGui::Button("+10")) { vel.x += 10.0f; drone.SetVelocity(vel); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::PopID();

    // Y velocity controls
    ImGui::PushID("vel_y");
    if (ImGui::Button("-10")) { vel.y -= 10.0f; drone.SetVelocity(vel); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    if (ImGui::Button("-1")) { vel.y -= 1.0f; drone.SetVelocity(vel); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    ImGui::Text("VY: %.1f", vel.y);
    ImGui::SameLine();
    if (ImGui::Button("+1")) { vel.y += 1.0f; drone.SetVelocity(vel); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    if (ImGui::Button("+10")) { vel.y += 10.0f; drone.SetVelocity(vel); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::PopID();

    // Stop velocity button
    if (ImGui::Button("Stop Movement"))
    {
        drone.SetVelocity(Math::Vec2(0.0f, 0.0f));
        SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName);
    }

    // Drag velocity control
    if (ImGui::DragFloat2("Drag Velocity", &vel.x, 0.1f, -500.0f, 500.0f))
    {
        drone.SetVelocity(vel);
        SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName);
    }

    // Size with +,- buttons
    Math::Vec2 size = drone.GetSize();
    ImGui::Text("Size: (%.1f, %.1f)", size.x, size.y);

    // X size controls
    ImGui::PushID("size_x");
    if (ImGui::Button("-10")) { size.x -= 10.0f; if (size.x < 1) size.x = 1; drone.SetSize(size); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    if (ImGui::Button("-1")) { size.x -= 1.0f; if (size.x < 1) size.x = 1; drone.SetSize(size); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    ImGui::Text("SX: %.1f", size.x);
    ImGui::SameLine();
    if (ImGui::Button("+1")) { size.x += 1.0f; drone.SetSize(size); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    if (ImGui::Button("+10")) { size.x += 10.0f; drone.SetSize(size); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::PopID();

    // Y size controls
    ImGui::PushID("size_y");
    if (ImGui::Button("-10")) { size.y -= 10.0f; if (size.y < 1) size.y = 1; drone.SetSize(size); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    if (ImGui::Button("-1")) { size.y -= 1.0f; if (size.y < 1) size.y = 1; drone.SetSize(size); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    ImGui::Text("SY: %.1f", size.y);
    ImGui::SameLine();
    if (ImGui::Button("+1")) { size.y += 1.0f; drone.SetSize(size); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    if (ImGui::Button("+10")) { size.y += 10.0f; drone.SetSize(size); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::PopID();

    // Drag size control
    if (ImGui::DragFloat2("Drag Size", &size.x, 0.1f, 1.0f, 200.0f))
    {
        drone.SetSize(size);
        SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName);
    }

    ImGui::Separator();

    // HP controls
    float hp = drone.GetHP();
    float maxHP = drone.GetMaxHP();
    ImGui::Text("HP: %.1f / %.1f", hp, maxHP);

    // HP modification
    if (ImGui::DragFloat("HP", &hp, 1.0f, 0.0f, maxHP))
    {
        drone.SetHP(hp);
        SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName);
    }

    if (ImGui::DragFloat("Max HP", &maxHP, 1.0f, 1.0f, 10000.0f))
    {
        drone.SetMaxHP(maxHP);
        SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName);
    }

    // Quick HP buttons
    ImGui::PushID("hp_buttons");
    if (ImGui::Button("Kill Drone"))
    {
        drone.SetHP(0.0f);
        SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName);
    }
    ImGui::SameLine();
    if (ImGui::Button("Full Heal"))
    {
        drone.SetHP(maxHP);
        SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName);
    }
    ImGui::PopID();

    ImGui::Separator();

    // Speed with +,- buttons
    float baseSpeed = drone.GetBaseSpeed();
    ImGui::Text("Base Speed: %.1f", baseSpeed);

    ImGui::PushID("speed");
    if (ImGui::Button("-50")) { baseSpeed -= 50.0f; if (baseSpeed < 0) baseSpeed = 0; drone.SetBaseSpeedDebug(baseSpeed); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    if (ImGui::Button("-10")) { baseSpeed -= 10.0f; if (baseSpeed < 0) baseSpeed = 0; drone.SetBaseSpeedDebug(baseSpeed); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    if (ImGui::Button("-1")) { baseSpeed -= 1.0f; if (baseSpeed < 0) baseSpeed = 0; drone.SetBaseSpeedDebug(baseSpeed); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    ImGui::Text("%.1f", baseSpeed);
    ImGui::SameLine();
    if (ImGui::Button("+1")) { baseSpeed += 1.0f; drone.SetBaseSpeedDebug(baseSpeed); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    if (ImGui::Button("+10")) { baseSpeed += 10.0f; drone.SetBaseSpeedDebug(baseSpeed); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    if (ImGui::Button("+50")) { baseSpeed += 50.0f; drone.SetBaseSpeedDebug(baseSpeed); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::PopID();

    // Drag speed control
    if (ImGui::DragFloat("Drag Base Speed", &baseSpeed, 1.0f, 0.0f, 1000.0f))
    {
        drone.SetBaseSpeedDebug(baseSpeed);
        SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName);
    }

    // States
    ImGui::Separator();
    ImGui::Text("States:");

    bool isDead = drone.IsDead();
    if (ImGui::Checkbox("Is Dead", &isDead))
    {
        drone.SetDead(isDead);
    }

    bool isHit = drone.IsHit();
    if (ImGui::Checkbox("Is Hit", &isHit))
    {
        drone.SetHit(isHit);
    }

    bool isAttacking = drone.IsAttacking();
    if (ImGui::Checkbox("Is Attacking", &isAttacking))
    {
        drone.SetAttacking(isAttacking);
    }

    // Damage timer with +,- buttons
    float damageTimer = drone.GetDamageTimer();
    ImGui::Text("Damage Timer: %.2f", damageTimer);

    ImGui::PushID("damage_timer");
    if (ImGui::Button("-0.5")) { damageTimer -= 0.5f; if (damageTimer < 0) damageTimer = 0; drone.SetDamageTimer(damageTimer); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    if (ImGui::Button("-0.1")) { damageTimer -= 0.1f; if (damageTimer < 0) damageTimer = 0; drone.SetDamageTimer(damageTimer); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    if (ImGui::Button("-0.01")) { damageTimer -= 0.01f; if (damageTimer < 0) damageTimer = 0; drone.SetDamageTimer(damageTimer); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    ImGui::Text("%.2f", damageTimer);
    ImGui::SameLine();
    if (ImGui::Button("+0.01")) { damageTimer += 0.01f; drone.SetDamageTimer(damageTimer); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    if (ImGui::Button("+0.1")) { damageTimer += 0.1f; drone.SetDamageTimer(damageTimer); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::SameLine();
    if (ImGui::Button("+0.5")) { damageTimer += 0.5f; drone.SetDamageTimer(damageTimer); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
    ImGui::PopID();

    // Reset damage timer button
    if (ImGui::Button("Reset Damage Timer"))
    {
        drone.ResetDamageTimerDebug();
        SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName);
    }

    // Drag damage timer control
    if (ImGui::DragFloat("Drag Damage Timer", &damageTimer, 0.01f, 0.0f, 2.0f))
    {
        drone.SetDamageTimer(damageTimer);
        SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName);
    }

    // Attack properties
    if (drone.IsAttacking())
    {
        ImGui::Separator();
        ImGui::Text("Attack Properties:");

        // Attack Radius with +,- buttons
        float attackRadius = drone.GetAttackRadius();
        ImGui::Text("Attack Radius: %.1f", attackRadius);

        ImGui::PushID("attack_radius");
        if (ImGui::Button("-50")) { attackRadius -= 50.0f; if (attackRadius < 10) attackRadius = 10; drone.SetAttackRadius(attackRadius); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
        ImGui::SameLine();
        if (ImGui::Button("-10")) { attackRadius -= 10.0f; if (attackRadius < 10) attackRadius = 10; drone.SetAttackRadius(attackRadius); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
        ImGui::SameLine();
        if (ImGui::Button("-1")) { attackRadius -= 1.0f; if (attackRadius < 10) attackRadius = 10; drone.SetAttackRadius(attackRadius); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
        ImGui::SameLine();
        ImGui::Text("%.1f", attackRadius);
        ImGui::SameLine();
        if (ImGui::Button("+1")) { attackRadius += 1.0f; drone.SetAttackRadius(attackRadius); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
        ImGui::SameLine();
        if (ImGui::Button("+10")) { attackRadius += 10.0f; drone.SetAttackRadius(attackRadius); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
        ImGui::SameLine();
        if (ImGui::Button("+50")) { attackRadius += 50.0f; drone.SetAttackRadius(attackRadius); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
        ImGui::PopID();

        // Drag attack radius control
        if (ImGui::DragFloat("Drag Attack Radius", &attackRadius, 1.0f, 10.0f, 500.0f))
        {
            drone.SetAttackRadius(attackRadius);
            SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName);
        }

        // Attack Angle with +,- buttons
        float attackAngle = drone.GetAttackAngle();
        ImGui::Text("Attack Angle: %.1f°", attackAngle);

        ImGui::PushID("attack_angle");
        if (ImGui::Button("-45°")) { attackAngle -= 45.0f; if (attackAngle < 0) attackAngle += 360; drone.SetAttackAngle(attackAngle); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
        ImGui::SameLine();
        if (ImGui::Button("-15°")) { attackAngle -= 15.0f; if (attackAngle < 0) attackAngle += 360; drone.SetAttackAngle(attackAngle); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
        ImGui::SameLine();
        if (ImGui::Button("-1°")) { attackAngle -= 1.0f; if (attackAngle < 0) attackAngle += 360; drone.SetAttackAngle(attackAngle); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
        ImGui::SameLine();
        ImGui::Text("%.1f°", attackAngle);
        ImGui::SameLine();
        if (ImGui::Button("+1°")) { attackAngle += 1.0f; if (attackAngle >= 360) attackAngle -= 360; drone.SetAttackAngle(attackAngle); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
        ImGui::SameLine();
        if (ImGui::Button("+15°")) { attackAngle += 15.0f; if (attackAngle >= 360) attackAngle -= 360; drone.SetAttackAngle(attackAngle); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
        ImGui::SameLine();
        if (ImGui::Button("+45°")) { attackAngle += 45.0f; if (attackAngle >= 360) attackAngle -= 360; drone.SetAttackAngle(attackAngle); SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName); }
        ImGui::PopID();

        // Drag attack angle control
        if (ImGui::DragFloat("Drag Attack Angle", &attackAngle, 1.0f, 0.0f, 360.0f))
        {
            drone.SetAttackAngle(attackAngle);
            SaveDroneState(drone, m_selectedDroneIndex, m_selectedMapName);
        }

        int direction = drone.GetAttackDirection();
        if (ImGui::SliderInt("Attack Direction", &direction, -1, 1))
        {
            drone.SetAttackDirection(direction);
        }
    }

    // Actions
    ImGui::Separator();
    if (ImGui::Button("Reset Position"))
    {
        drone.SetPosition(Math::Vec2(400.0f, 300.0f));
        drone.SetVelocity(Math::Vec2(0.0f, 0.0f));
    }

    ImGui::SameLine();
    if (ImGui::Button("Stop Movement"))
    {
        drone.SetVelocity(Math::Vec2(0.0f, 0.0f));
    }

    ImGui::SameLine();
    if (ImGui::Button("Revive"))
    {
        drone.SetDead(false);
        drone.SetHit(false);
        drone.ResetHitTimer();
        drone.ResetDamageTimerDebug();
    }
}

void ImguiManager::AddMapDroneManager(const std::string& mapName, DroneManager* manager)
{
    m_mapDroneManagers.emplace_back(mapName, manager);
}

void ImguiManager::SaveDroneState(Drone& drone, int index, const std::string& mapName)
{
    if (!m_configManager)
        return;

    LiveDroneState state;
    state.droneIndex = index;
    state.mapName = mapName;
    state.positionX = drone.GetPosition().x;
    state.positionY = drone.GetPosition().y;
    state.velocityX = drone.GetVelocity().x;
    state.velocityY = drone.GetVelocity().y;
    state.sizeX = drone.GetSize().x;
    state.sizeY = drone.GetSize().y;
    state.baseSpeed = drone.GetBaseSpeed();
    state.attackRadius = drone.GetAttackRadius();
    state.attackAngle = drone.GetAttackAngle();
    state.attackDirection = drone.GetAttackDirection();
    state.damageTimer = drone.GetDamageTimer();
    state.maxHP = drone.GetMaxHP();  // Only save maxHP
    state.isDebugMode = drone.IsDebugMode();
    state.isDead = drone.IsDead();
    state.isHit = drone.IsHit();
    state.isAttacking = drone.IsAttacking();

    m_configManager->UpdateLiveState(state);
}

void ImguiManager::LoadDroneState(Drone& drone, int index, const std::string& mapName)
{
    if (!m_configManager)
        return;

    m_configManager->ApplyLiveStateToDrone(mapName, index, drone);
}

void ImguiManager::DrawRobotProperties(Robot& robot, int index)
{
    ImGui::Text("Robot #%d Properties:", index);
    ImGui::Separator();

    bool modified = false;

    // Position
    Math::Vec2 pos = robot.GetPosition();
    float posArray[2] = { pos.x, pos.y };
    if (ImGui::DragFloat2("Position", posArray, 1.0f))
    {
        robot.SetPosition({ posArray[0], posArray[1] });
        modified = true;
    }

    // Size
    Math::Vec2 size = robot.GetSize();
    float sizeArray[2] = { size.x, size.y };
    if (ImGui::DragFloat2("Size", sizeArray, 1.0f, 10.0f, 1000.0f))
    {
        robot.SetSize({ sizeArray[0], sizeArray[1] });
        modified = true;
    }

    // HP
    float hp = robot.GetHP();
    if (ImGui::DragFloat("HP", &hp, 1.0f, 0.0f, robot.GetMaxHP()))
    {
        robot.SetHP(hp);
        modified = true;
    }

    // Max HP
    float maxHP = robot.GetMaxHP();
    if (ImGui::DragFloat("Max HP", &maxHP, 1.0f, 1.0f, 1000.0f))
    {
        robot.SetMaxHP(maxHP);
        modified = true;
    }

    // Direction
    float dirX = robot.GetDirectionX();
    if (ImGui::DragFloat("Direction X", &dirX, 0.1f, -1.0f, 1.0f))
    {
        robot.SetDirectionX(dirX);
        modified = true;
    }

    // Save state if modified
    if (modified)
    {
        SaveRobotState(robot, index, m_selectedMapName);
    }

    // State (read-only display)
    ImGui::Separator();
    ImGui::Text("Current State:");
    RobotState state = robot.GetState();
    const char* stateNames[] = { "Patrol", "Chase", "Retreat", "Windup", "Attack", "Recover", "Stagger", "Dead" };
    ImGui::Text("%s", stateNames[static_cast<int>(state)]);

    // Constants (read-only)
    ImGui::Separator();
    ImGui::Text("Constants (Read-Only):");
    ImGui::Text("Patrol Speed: %.1f", robot.GetPatrolSpeed());
    ImGui::Text("Chase Speed: %.1f", robot.GetChaseSpeed());
    ImGui::Text("Detection Range: %.1f", robot.GetDetectionRange());
    ImGui::Text("Attack Range: %.1f", robot.GetAttackRange());

    // Actions
    ImGui::Separator();
    if (ImGui::Button("Kill Robot"))
    {
        robot.TakeDamage(robot.GetMaxHP());
        SaveRobotState(robot, index, m_selectedMapName);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Full Heal"))
    {
        robot.SetHP(robot.GetMaxHP());
        SaveRobotState(robot, index, m_selectedMapName);
    }
}

void ImguiManager::SaveRobotState(Robot& robot, int index, const std::string& mapName)
{
    if (!m_robotConfigManager)
        return;

    LiveRobotState state;
    state.robotIndex = index;
    state.mapName = mapName;
    
    Math::Vec2 pos = robot.GetPosition();
    state.positionX = pos.x;
    state.positionY = pos.y;
    
    Math::Vec2 size = robot.GetSize();
    state.sizeX = size.x;
    state.sizeY = size.y;
    
    state.hp = robot.GetHP();
    state.maxHP = robot.GetMaxHP();
    state.directionX = robot.GetDirectionX();
    state.state = static_cast<int>(robot.GetState());

    m_robotConfigManager->UpdateLiveState(state);
}

void ImguiManager::LoadRobotState(Robot& robot, int index, const std::string& mapName)
{
    if (!m_robotConfigManager)
        return;

    m_robotConfigManager->ApplyLiveStateToRobot(mapName, index, robot);
}

void ImguiManager::EndFrame()
{
    // Note: Rendering is now handled in DrawDebugWindow()
}

