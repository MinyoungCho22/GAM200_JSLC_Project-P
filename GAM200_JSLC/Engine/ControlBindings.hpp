#pragma once

#include "Input.hpp"
#include <array>
#include <optional>
#include <string>
#include <vector>

struct GLFWwindow;

enum class ControlAction : uint8_t
{
    MoveLeft = 0,
    MoveRight,
    Jump,
    Dash,
    Attack,
    PulseAbsorb,
    PulseDetonate,
    Count
};

enum class BindKind : uint8_t
{
    None = 0,
    Key,
    MouseButton,
    GamepadButton,
    /// True when gamepad axis `code` is past deadzone in direction `axisSign` (-1 or +1).
    GamepadAxisHalf
};

struct PhysicalBinding
{
    BindKind kind = BindKind::None;
    int code = 0;
    int axisSign = 0;

    bool operator==(const PhysicalBinding& o) const
    {
        return kind == o.kind && code == o.code && axisSign == o.axisSign;
    }
};

class ControlBindings
{
public:
    void LoadOrDefaults(const std::string& path);
    bool Save() const;

    void BeginRebind(ControlAction action, GLFWwindow* mainWindow);
    void CancelRebind();
    bool IsRebinding() const { return m_rebinding.has_value(); }
    ControlAction RebindTarget() const { return m_rebinding.value_or(ControlAction::Jump); }

    /// Call after Input::Update each frame; uses main window for keyboard/mouse capture edges.
    void TickRebindCapture(GLFWwindow* mainWindow, Input::Input& input);

    bool IsActionPressed(ControlAction a, const Input::Input& input) const;
    bool IsActionTriggered(ControlAction a, const Input::Input& input) const;

    float GetMoveHorizontalAxis(const Input::Input& input) const;

    std::string FormatActionLabel(ControlAction a) const;
    std::string FormatBindingList(ControlAction a) const;

    const std::vector<PhysicalBinding>& GetBindings(ControlAction a) const { return m_slots[static_cast<size_t>(a)]; }

private:
    void ApplyDefaults();
    bool LoadFromJsonFile(const std::string& path);
    void ToJsonFile(const std::string& path) const;

    bool MatchDown(const PhysicalBinding& b, const Input::Input& input) const;
    bool MatchTriggered(const PhysicalBinding& b, const Input::Input& input) const;

    void RemoveBindingFromOthers(const PhysicalBinding& b, ControlAction except);
    void SnapshotBaseline(GLFWwindow* mainWindow);
    void FinishCapture(PhysicalBinding b);

    std::string m_path;
    std::array<std::vector<PhysicalBinding>, static_cast<size_t>(ControlAction::Count)> m_slots{};

    std::optional<ControlAction> m_rebinding;

    std::array<unsigned char, 349> m_capKeyPrev{};
    std::array<int, 8> m_capMousePrev{};
    GLFWgamepadstate m_capGpPrev{};
    bool m_capHadGamepad = false;
};
