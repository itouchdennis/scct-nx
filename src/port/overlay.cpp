#include "port/overlay.h"

#include <SDL.h>
#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

#include "app/app.h"
#include "core/log.h"
#include "image_symbols.h"
#include "port/pads.h"

namespace port {

namespace {

// the game draws the frame rate in its own font where the image has the hook for it (Double Agent's Magma pages);
// elsewhere the overlay does
constexpr bool kOverlayFps = std::string_view(IMAGE_XBE) == "sc3.xbe";

constexpr WORD kCombo = XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER | XINPUT_GAMEPAD_DPAD_RIGHT;
bool g_combo_held = false;  // main thread

std::mutex g_keys_mu;
std::vector<std::pair<ImGuiKey, bool>> g_keys;

// render thread
bool g_was_open = false;
WORD g_last = 0;     // the buttons held at the last frame
WORD g_ignored = 0;  // held when the panel opened: the combination's buttons, not the panel's until let go
std::chrono::steady_clock::time_point g_then;

WORD Held(XINPUT_GAMEPAD* first) {
    WORD held = 0;
    bool found = false;
    for (int slot = 0; slot < 4; slot++) {
        XINPUT_GAMEPAD g;
        if (!ReadPad(slot, g)) continue;
        held |= g.wButtons;
        if (!found && first) *first = g;
        found = true;
    }
    if (!found && first) *first = {};
    return held;
}

ImGuiKey KeyOf(int scancode) {
    switch (scancode) {
        case SDL_SCANCODE_UP: return ImGuiKey_UpArrow;
        case SDL_SCANCODE_DOWN: return ImGuiKey_DownArrow;
        case SDL_SCANCODE_LEFT: return ImGuiKey_LeftArrow;
        case SDL_SCANCODE_RIGHT: return ImGuiKey_RightArrow;
        case SDL_SCANCODE_RETURN: return ImGuiKey_Enter;
        case SDL_SCANCODE_KP_ENTER: return ImGuiKey_KeypadEnter;
        case SDL_SCANCODE_SPACE: return ImGuiKey_Space;
        case SDL_SCANCODE_TAB: return ImGuiKey_Tab;
        case SDL_SCANCODE_PAGEUP: return ImGuiKey_PageUp;
        case SDL_SCANCODE_PAGEDOWN: return ImGuiKey_PageDown;
        case SDL_SCANCODE_HOME: return ImGuiKey_Home;
        case SDL_SCANCODE_END: return ImGuiKey_End;
        case SDL_SCANCODE_DELETE: return ImGuiKey_Delete;
        case SDL_SCANCODE_BACKSPACE: return ImGuiKey_Backspace;
        case SDL_SCANCODE_LSHIFT: return ImGuiKey_LeftShift;
        case SDL_SCANCODE_RSHIFT: return ImGuiKey_RightShift;
        default: return ImGuiKey_None;
    }
}

// the controllers as ImGui's gamepad: Xbox labels, so A (Nintendo's A with the Switch layout) activates, B goes back
void FeedPads(ImGuiIO& io, WORD held, const XINPUT_GAMEPAD& g) {
    constexpr std::pair<WORD, ImGuiKey> kButtons[] = {
        {XINPUT_GAMEPAD_A, ImGuiKey_GamepadFaceDown},       {XINPUT_GAMEPAD_B, ImGuiKey_GamepadFaceRight},
        {XINPUT_GAMEPAD_X, ImGuiKey_GamepadFaceLeft},       {XINPUT_GAMEPAD_Y, ImGuiKey_GamepadFaceUp},
        {XINPUT_GAMEPAD_DPAD_UP, ImGuiKey_GamepadDpadUp},   {XINPUT_GAMEPAD_DPAD_DOWN, ImGuiKey_GamepadDpadDown},
        {XINPUT_GAMEPAD_DPAD_LEFT, ImGuiKey_GamepadDpadLeft}, {XINPUT_GAMEPAD_DPAD_RIGHT, ImGuiKey_GamepadDpadRight},
        {XINPUT_GAMEPAD_LEFT_SHOULDER, ImGuiKey_GamepadL1}, {XINPUT_GAMEPAD_RIGHT_SHOULDER, ImGuiKey_GamepadR1},
        {XINPUT_GAMEPAD_START, ImGuiKey_GamepadStart},      {XINPUT_GAMEPAD_BACK, ImGuiKey_GamepadBack},
    };
    g_ignored &= held;
    WORD live = held & ~g_ignored;
    for (auto [mask, key] : kButtons) io.AddKeyEvent(key, (live & mask) != 0);
    // the left stick past a third of the way steps as the D-pad does
    auto axis = [&](ImGuiKey key, int v) { io.AddKeyAnalogEvent(key, v > 10000, std::clamp((v - 8000) / 24000.0f, 0.0f, 1.0f)); };
    axis(ImGuiKey_GamepadLStickRight, g.sThumbLX);
    axis(ImGuiKey_GamepadLStickLeft, -int(g.sThumbLX));
    axis(ImGuiKey_GamepadLStickUp, g.sThumbLY);
    axis(ImGuiKey_GamepadLStickDown, -int(g.sThumbLY));
}

void DrawFps() {
    char text[32];
    std::snprintf(text, sizeof text, "FPS: %.1f", double(app::g_fps.load()));
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float em = ImGui::GetFontSize();
    ImVec2 at(em * 0.6f, em * 0.4f);
    dl->AddText(ImVec2(at.x + 1, at.y + 1), IM_COL32(0, 0, 0, 255), text);
    dl->AddText(at, IM_COL32(255, 255, 255, 255), text);
}

}  // namespace

void OverlayInit() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    io.BackendPlatformName = "scct-nx";
    io.BackendRendererName = "scct-nx vk";
    // the font made at twice ImGui's size, so app::DrawUi's scale draws it down rather than up
    ImFontConfig font;
    font.SizePixels = 26.0f;
    io.Fonts->AddFontDefault(&font);
    io.Fonts->Build();
    g_then = std::chrono::steady_clock::now();
}

void OverlayPads() {
    bool held = (Held(nullptr) & kCombo) == kCombo;
    if (held && !g_combo_held) {
        std::lock_guard lock(app::UiLock());
        app::ToggleMenu();
        LOGI("overlay: settings %s", app::MenuOpen() ? "open" : "closed");
    }
    g_combo_held = held;
}

void OverlayKey(int scancode, bool down) {
    ImGuiKey key = KeyOf(scancode);
    if (key == ImGuiKey_None) return;
    std::lock_guard lock(g_keys_mu);
    g_keys.emplace_back(key, down);
}

ImDrawData* OverlayFrame(uint32_t width, uint32_t height) {
    if (!ImGui::GetCurrentContext()) return nullptr;
    std::lock_guard lock(app::UiLock());
    ImGuiIO& io = ImGui::GetIO();
    XINPUT_GAMEPAD pad;
    WORD held = Held(&pad);
    bool open = app::MenuOpen();
    if (open && !g_was_open) g_ignored = held;
    // B closes the panel where it is not answering a bind or closing a list
    WORD pressed = held & ~g_last & ~g_ignored;
    g_last = held;
    if (open && (pressed & XINPUT_GAMEPAD_B) && !app::Capturing() &&
        !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
        app::ToggleMenu();
        open = false;
    }
    g_was_open = open;
    bool fps = kOverlayFps && app::Get().fps_counter;
    std::vector<std::pair<ImGuiKey, bool>> keys;
    {
        std::lock_guard keys_lock(g_keys_mu);
        keys.swap(g_keys);
    }
    if (!open && !fps) return nullptr;
    auto now = std::chrono::steady_clock::now();
    io.DeltaTime = std::clamp(std::chrono::duration<float>(now - g_then).count(), 1e-4f, 0.1f);
    g_then = now;
    io.DisplaySize = ImVec2(float(width), float(height));
    for (auto [key, down] : keys) io.AddKeyEvent(key, down);
    FeedPads(io, open ? held : 0, pad);
    ImGui::NewFrame();
    app::DrawUi();
    if (fps) DrawFps();
    ImGui::Render();
    return ImGui::GetDrawData();
}

}  // namespace port
