#include "ControlBindings.hpp"
#include "../ThirdParty/json/nlohmann_json.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace
{
constexpr float kMoveStickDeadzone = 0.2f;
constexpr float kAxisHalfDeadzone = 0.35f;

const char* ActionJsonKey(ControlAction a)
{
    switch (a)
    {
    case ControlAction::MoveLeft: return "move_left";
    case ControlAction::MoveRight: return "move_right";
    case ControlAction::Jump: return "jump";
    case ControlAction::Dash: return "dash";
    case ControlAction::Attack: return "attack";
    case ControlAction::PulseAbsorb: return "pulse_absorb";
    case ControlAction::PulseDetonate: return "pulse_detonate";
    default: return "";
    }
}

bool ParseBinding(const nlohmann::json& j, PhysicalBinding& out)
{
    if (!j.is_object() || !j.contains("type"))
        return false;
    const std::string t = j["type"].get<std::string>();
    if (t == "key")
    {
        out.kind = BindKind::Key;
        out.code = j.value("key", 0);
        out.axisSign = 0;
        return true;
    }
    if (t == "mouse")
    {
        out.kind = BindKind::MouseButton;
        out.code = j.value("button", 0);
        out.axisSign = 0;
        return true;
    }
    if (t == "gamepad_button")
    {
        out.kind = BindKind::GamepadButton;
        out.code = j.value("button", 0);
        out.axisSign = 0;
        return true;
    }
    if (t == "gamepad_axis")
    {
        out.kind = BindKind::GamepadAxisHalf;
        out.code = j.value("axis", 0);
        out.axisSign = j.value("sign", 1);
        if (out.axisSign != -1 && out.axisSign != 1)
            out.axisSign = 1;
        return true;
    }
    return false;
}

nlohmann::json SerializeBinding(const PhysicalBinding& b)
{
    nlohmann::json j;
    switch (b.kind)
    {
    case BindKind::Key:
        j["type"] = "key";
        j["key"] = b.code;
        break;
    case BindKind::MouseButton:
        j["type"] = "mouse";
        j["button"] = b.code;
        break;
    case BindKind::GamepadButton:
        j["type"] = "gamepad_button";
        j["button"] = b.code;
        break;
    case BindKind::GamepadAxisHalf:
        j["type"] = "gamepad_axis";
        j["axis"] = b.code;
        j["sign"] = b.axisSign;
        break;
    default:
        j["type"] = "none";
        break;
    }
    return j;
}

const char* MouseName(int b)
{
    if (b == 0) return "Mouse Left";
    if (b == 1) return "Mouse Right";
    if (b == 2) return "Mouse Middle";
    return "Mouse";
}

const char* GpButtonName(int btn)
{
    switch (btn)
    {
    case GLFW_GAMEPAD_BUTTON_A: return "Pad A";
    case GLFW_GAMEPAD_BUTTON_B: return "Pad B";
    case GLFW_GAMEPAD_BUTTON_X: return "Pad X";
    case GLFW_GAMEPAD_BUTTON_Y: return "Pad Y";
    case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER: return "Pad LB";
    case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: return "Pad RB";
    case GLFW_GAMEPAD_BUTTON_BACK: return "Pad Back";
    case GLFW_GAMEPAD_BUTTON_START: return "Pad Start";
    case GLFW_GAMEPAD_BUTTON_GUIDE: return "Pad Guide";
    case GLFW_GAMEPAD_BUTTON_LEFT_THUMB: return "Pad L3";
    case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB: return "Pad R3";
    case GLFW_GAMEPAD_BUTTON_DPAD_UP: return "Pad D-Up";
    case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT: return "Pad D-Right";
    case GLFW_GAMEPAD_BUTTON_DPAD_DOWN: return "Pad D-Down";
    case GLFW_GAMEPAD_BUTTON_DPAD_LEFT: return "Pad D-Left";
    default: return "Pad Btn";
    }
}

std::string FormatOne(const PhysicalBinding& b)
{
    switch (b.kind)
    {
    case BindKind::Key:
        return std::string("Key ") + std::to_string(b.code);
    case BindKind::MouseButton:
        return MouseName(b.code);
    case BindKind::GamepadButton:
        return GpButtonName(b.code);
    case BindKind::GamepadAxisHalf:
        return std::string("Axis ") + std::to_string(b.code) + (b.axisSign < 0 ? " -" : " +");
    default:
        return "-";
    }
}
} // namespace

void ControlBindings::ApplyDefaults()
{
    m_slots.fill({});

    m_slots[static_cast<size_t>(ControlAction::MoveLeft)] = { { BindKind::Key, GLFW_KEY_A, 0 } };
    m_slots[static_cast<size_t>(ControlAction::MoveRight)] = { { BindKind::Key, GLFW_KEY_D, 0 } };
    m_slots[static_cast<size_t>(ControlAction::Jump)] = {
        { BindKind::Key, GLFW_KEY_SPACE, 0 },
        { BindKind::Key, GLFW_KEY_W, 0 },
        { BindKind::GamepadButton, GLFW_GAMEPAD_BUTTON_A, 0 },
    };
    m_slots[static_cast<size_t>(ControlAction::Dash)] = {
        { BindKind::Key, GLFW_KEY_LEFT_SHIFT, 0 },
        { BindKind::GamepadButton, GLFW_GAMEPAD_BUTTON_X, 0 },
    };
    m_slots[static_cast<size_t>(ControlAction::Attack)] = {
        { BindKind::MouseButton, 0, 0 },
        { BindKind::GamepadButton, GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER, 0 },
    };
    m_slots[static_cast<size_t>(ControlAction::PulseAbsorb)] = {
        { BindKind::MouseButton, 1, 0 },
        { BindKind::GamepadButton, GLFW_GAMEPAD_BUTTON_LEFT_BUMPER, 0 },
    };
    m_slots[static_cast<size_t>(ControlAction::PulseDetonate)] = {
        { BindKind::Key, GLFW_KEY_Q, 0 },
        { BindKind::GamepadButton, GLFW_GAMEPAD_BUTTON_Y, 0 },
    };
}

bool ControlBindings::LoadFromJsonFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f)
        return false;
    nlohmann::json root;
    try
    {
        f >> root;
    }
    catch (...)
    {
        return false;
    }
    if (!root.is_object() || !root.contains("bindings"))
        return false;

    m_slots.fill({});
    const auto& bag = root["bindings"];
    for (size_t i = 0; i < static_cast<size_t>(ControlAction::Count); ++i)
    {
        const auto a = static_cast<ControlAction>(i);
        const char* k = ActionJsonKey(a);
        if (!bag.contains(k))
            continue;
        const auto& arr = bag[k];
        if (!arr.is_array())
            continue;
        for (const auto& item : arr)
        {
            PhysicalBinding pb{};
            if (ParseBinding(item, pb) && pb.kind != BindKind::None)
                m_slots[i].push_back(pb);
        }
    }
    return true;
}

void ControlBindings::ToJsonFile(const std::string& path) const
{
    nlohmann::json root;
    root["version"] = 1;
    nlohmann::json bag = nlohmann::json::object();
    for (size_t i = 0; i < static_cast<size_t>(ControlAction::Count); ++i)
    {
        const auto a = static_cast<ControlAction>(i);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& b : m_slots[i])
        {
            if (b.kind != BindKind::None)
                arr.push_back(SerializeBinding(b));
        }
        bag[ActionJsonKey(a)] = std::move(arr);
    }
    root["bindings"] = std::move(bag);

    std::ofstream f(path);
    if (f)
        f << root.dump(2);
}

void ControlBindings::LoadOrDefaults(const std::string& path)
{
    m_path = path;
    if (!LoadFromJsonFile(path))
    {
        ApplyDefaults();
        return;
    }

    // Fill in defaults for any newly added actions that are missing from the JSON.
    // This prevents new keybindings from silently failing when an old save file exists.
    ControlBindings defaults;
    defaults.ApplyDefaults();
    for (size_t i = 0; i < static_cast<size_t>(ControlAction::Count); ++i)
    {
        if (m_slots[i].empty())
            m_slots[i] = defaults.m_slots[i];
    }
}

bool ControlBindings::Save() const
{
    if (m_path.empty())
        return false;
    ToJsonFile(m_path);
    return true;
}

bool ControlBindings::MatchDown(const PhysicalBinding& b, const Input::Input& input) const
{
    switch (b.kind)
    {
    case BindKind::Key:
        return input.IsGlfwKeyPressed(b.code);
    case BindKind::MouseButton:
        if (b.code == 0)
            return input.IsMouseButtonPressed(Input::MouseButton::Left);
        if (b.code == 1)
            return input.IsMouseButtonPressed(Input::MouseButton::Right);
        if (b.code == 2)
            return input.IsMouseButtonPressed(Input::MouseButton::Middle);
        return false;
    case BindKind::GamepadButton:
    {
        if (!input.IsGamepadConnected())
            return false;
        return input.GamepadCurrent().buttons[b.code] != 0;
    }
    case BindKind::GamepadAxisHalf:
    {
        if (!input.IsGamepadConnected())
            return false;
        float v = input.GamepadCurrent().axes[b.code];
        if (b.axisSign < 0)
            return v < -kAxisHalfDeadzone;
        return v > kAxisHalfDeadzone;
    }
    default:
        return false;
    }
}

bool ControlBindings::MatchTriggered(const PhysicalBinding& b, const Input::Input& input) const
{
    switch (b.kind)
    {
    case BindKind::Key:
        return input.IsGlfwKeyTriggered(b.code);
    case BindKind::MouseButton:
        if (b.code == 0)
            return input.IsMouseButtonTriggered(Input::MouseButton::Left);
        if (b.code == 1)
            return input.IsMouseButtonTriggered(Input::MouseButton::Right);
        if (b.code == 2)
            return input.IsMouseButtonTriggered(Input::MouseButton::Middle);
        return false;
    case BindKind::GamepadButton:
    {
        if (!input.IsGamepadConnected())
            return false;
        return input.GamepadCurrent().buttons[b.code] != 0
            && input.GamepadPrevious().buttons[b.code] == 0;
    }
    case BindKind::GamepadAxisHalf:
    {
        if (!input.IsGamepadConnected())
            return false;
        float v = input.GamepadCurrent().axes[b.code];
        float pv = input.GamepadPrevious().axes[b.code];
        bool now = (b.axisSign < 0) ? (v < -kAxisHalfDeadzone) : (v > kAxisHalfDeadzone);
        bool was = (b.axisSign < 0) ? (pv < -kAxisHalfDeadzone) : (pv > kAxisHalfDeadzone);
        return now && !was;
    }
    default:
        return false;
    }
}

bool ControlBindings::IsActionPressed(ControlAction a, const Input::Input& input) const
{
    for (const auto& b : m_slots[static_cast<size_t>(a)])
    {
        if (MatchDown(b, input))
            return true;
    }
    return false;
}

bool ControlBindings::IsActionTriggered(ControlAction a, const Input::Input& input) const
{
    for (const auto& b : m_slots[static_cast<size_t>(a)])
    {
        if (MatchTriggered(b, input))
            return true;
    }
    return false;
}

float ControlBindings::GetMoveHorizontalAxis(const Input::Input& input) const
{
    float k = 0.0f;
    for (const auto& b : m_slots[static_cast<size_t>(ControlAction::MoveLeft)])
    {
        if (MatchDown(b, input))
            k -= 1.0f;
    }
    for (const auto& b : m_slots[static_cast<size_t>(ControlAction::MoveRight)])
    {
        if (MatchDown(b, input))
            k += 1.0f;
    }
    k = (std::max)(-1.0f, (std::min)(1.0f, k));

    float stickX = 0.0f;
    if (input.IsGamepadConnected())
        stickX = input.GamepadCurrent().axes[GLFW_GAMEPAD_AXIS_LEFT_X];

    if (std::fabs(stickX) > kMoveStickDeadzone && std::fabs(k) < 0.01f)
        return (std::max)(-1.0f, (std::min)(1.0f, stickX));
    return k;
}

std::string ControlBindings::FormatActionLabel(ControlAction a) const
{
    switch (a)
    {
    case ControlAction::MoveLeft: return "Move Left";
    case ControlAction::MoveRight: return "Move Right";
    case ControlAction::Jump: return "Jump";
    case ControlAction::Dash: return "Dash";
    case ControlAction::Attack: return "Attack / Interact";
    case ControlAction::PulseAbsorb: return "Pulse Absorb";
    case ControlAction::PulseDetonate: return "Pulse Detonate";
    default: return "?";
    }
}

std::string ControlBindings::FormatBindingList(ControlAction a) const
{
    const auto& v = m_slots[static_cast<size_t>(a)];
    if (v.empty())
        return "(none)";
    std::string s = FormatOne(v[0]);
    for (size_t i = 1; i < v.size(); ++i)
    {
        s += ", ";
        s += FormatOne(v[i]);
    }
    return s;
}

void ControlBindings::RemoveBindingFromOthers(const PhysicalBinding& b, ControlAction except)
{
    if (b.kind == BindKind::None)
        return;
    for (size_t i = 0; i < m_slots.size(); ++i)
    {
        if (static_cast<ControlAction>(i) == except)
            continue;
        auto& vec = m_slots[i];
        vec.erase(std::remove(vec.begin(), vec.end(), b), vec.end());
    }
}

void ControlBindings::SnapshotBaseline(GLFWwindow* mainWindow)
{
    const int keyLast = (std::min)(GLFW_KEY_LAST, static_cast<int>(m_capKeyPrev.size()) - 1);
    for (int k = 0; k <= keyLast; ++k)
        m_capKeyPrev[static_cast<size_t>(k)] = static_cast<unsigned char>(glfwGetKey(mainWindow, k));
    for (int m = 0; m < 8; ++m)
        m_capMousePrev[static_cast<size_t>(m)] = glfwGetMouseButton(mainWindow, m);

    m_capHadGamepad = glfwJoystickPresent(GLFW_JOYSTICK_1) && glfwJoystickIsGamepad(GLFW_JOYSTICK_1);
    if (m_capHadGamepad)
        glfwGetGamepadState(GLFW_JOYSTICK_1, &m_capGpPrev);
    else
        std::memset(&m_capGpPrev, 0, sizeof(m_capGpPrev));
}

void ControlBindings::BeginRebind(ControlAction action, GLFWwindow* mainWindow)
{
    m_rebinding = action;
    if (mainWindow)
        SnapshotBaseline(mainWindow);
}

void ControlBindings::CancelRebind()
{
    m_rebinding.reset();
}

void ControlBindings::FinishCapture(PhysicalBinding b)
{
    if (!m_rebinding.has_value())
        return;
    const ControlAction a = m_rebinding.value();
    RemoveBindingFromOthers(b, a);
    m_slots[static_cast<size_t>(a)].clear();
    m_slots[static_cast<size_t>(a)].push_back(b);
    m_rebinding.reset();
    Save();
}

void ControlBindings::TickRebindCapture(GLFWwindow* mainWindow, Input::Input& input)
{
    (void)input;
    if (!m_rebinding.has_value() || !mainWindow)
        return;

    const int keyLast = (std::min)(GLFW_KEY_LAST, static_cast<int>(m_capKeyPrev.size()) - 1);
    for (int k = 0; k <= keyLast; ++k)
    {
        const int cur = glfwGetKey(mainWindow, k);
        if (cur == GLFW_PRESS && m_capKeyPrev[static_cast<size_t>(k)] == GLFW_RELEASE)
        {
            if (k == GLFW_KEY_ESCAPE)
            {
                CancelRebind();
                return;
            }
            PhysicalBinding pb{ BindKind::Key, k, 0 };
            FinishCapture(pb);
            return;
        }
    }

    for (int mb = 0; mb < 3; ++mb)
    {
        const int cur = glfwGetMouseButton(mainWindow, mb);
        if (cur == GLFW_PRESS && m_capMousePrev[static_cast<size_t>(mb)] == GLFW_RELEASE)
        {
            FinishCapture({ BindKind::MouseButton, mb, 0 });
            return;
        }
    }

    if (glfwJoystickPresent(GLFW_JOYSTICK_1) && glfwJoystickIsGamepad(GLFW_JOYSTICK_1))
    {
        GLFWgamepadstate st{};
        if (glfwGetGamepadState(GLFW_JOYSTICK_1, &st) == GLFW_TRUE)
        {
            for (int btn = 0; btn <= GLFW_GAMEPAD_BUTTON_LAST; ++btn)
            {
                if (st.buttons[btn] != 0 && m_capGpPrev.buttons[btn] == 0)
                {
                    FinishCapture({ BindKind::GamepadButton, btn, 0 });
                    return;
                }
            }
            for (int ax = 0; ax <= GLFW_GAMEPAD_AXIS_LAST; ++ax)
            {
                float v = st.axes[ax];
                float pv = m_capGpPrev.axes[ax];
                bool posNow = v > kAxisHalfDeadzone;
                bool posWas = pv > kAxisHalfDeadzone;
                bool negNow = v < -kAxisHalfDeadzone;
                bool negWas = pv < -kAxisHalfDeadzone;
                if (posNow && !posWas)
                {
                    FinishCapture({ BindKind::GamepadAxisHalf, ax, 1 });
                    return;
                }
                if (negNow && !negWas)
                {
                    FinishCapture({ BindKind::GamepadAxisHalf, ax, -1 });
                    return;
                }
            }
        }
    }

    for (int k = 0; k <= keyLast; ++k)
        m_capKeyPrev[static_cast<size_t>(k)] = static_cast<unsigned char>(glfwGetKey(mainWindow, k));
    for (int m = 0; m < 8; ++m)
        m_capMousePrev[static_cast<size_t>(m)] = glfwGetMouseButton(mainWindow, m);
    if (glfwJoystickPresent(GLFW_JOYSTICK_1) && glfwJoystickIsGamepad(GLFW_JOYSTICK_1))
        glfwGetGamepadState(GLFW_JOYSTICK_1, &m_capGpPrev);
    else
        std::memset(&m_capGpPrev, 0, sizeof(m_capGpPrev));
}
