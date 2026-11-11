// Gyro aiming and the stick's sensitivity, as Double Agent's (src/game/gyro_look.cpp, mouse_pitch.cpp): the gyro's
// turn is added to the player controller's rotation before the camera update reads it, so the camera's own pitch
// clamp holds it, and the stick turns by aTurn * yaw speed and aLookUp * pitch speed, which the sensitivities scale
// for the update. Chaos Theory's EPlayerCam is Double Agent's at other offsets. Aiming (the gun raised, the sniper
// scope) is the controller's script state, as Double Agent's views.cpp reads it: a targeting or sniping one.
#include <bit>
#include <cctype>
#include <cmath>
#include <string>
#include <unordered_map>

#include "app/app.h"
#include "core/gyro.h"
#include "core/input.h"
#include "core/log.h"
#include "kernel/memory.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

// EPlayerCam: +0x1198 its controller, +0x127C its yaw speed, +0x1280 its pitch speed; controller: +0xF4 its
// rotation's pitch, up positive, +0xF8 its yaw, right positive (65536 a turn)
constexpr uint32_t kController = 0x1198;
constexpr uint32_t kYawSpeed = 0x127C, kPitchSpeed = 0x1280;
constexpr uint32_t kPitch = 0xF4;
constexpr uint32_t kYaw = 0xF8;
constexpr float kUnitsPerDegree = 65536.0f / 360.0f;

// object: +0xC state frame, +0x20 name, +0x24 class; state frame: +0x1C state, as Double Agent's (unreal.cpp). Unlike
// Double Agent's, Chaos Theory's names are indices into FName::Names (a TArray: +0 data, +4 count), its entries the
// name as UTF-16 at +0xC
constexpr uint32_t kStateFrame = 0x0C, kName = 0x20, kClass = 0x24, kFrameState = 0x1C;
constexpr uint32_t kNames = 0x5CD588;
constexpr uint32_t kEntryText = 0xC;
constexpr uint32_t kMaxName = 64;

// a read of guest memory that may not be there: 0 then
uint32_t Read(uint32_t address) { return address >= 0x10000 && mem::IsCommitted(address) ? R32(address) : 0; }

// the name's text, empty for an index the table has not
std::string NameText(uint32_t index) {
    uint32_t data = Read(kNames), count = Read(kNames + 4);
    uint32_t entry = data && index < count ? Read(data + index * 4) : 0;
    std::string out;
    if (!entry || !mem::IsCommitted(entry + kEntryText)) return out;
    for (uint32_t i = 0; i < kMaxName; i++) {
        uint16_t c = R16(entry + kEntryText + i * 2);
        if (!c) break;
        out += c < 0x80 ? char(c) : '?';
    }
    return out;
}

uint32_t StateOf(uint32_t object) {
    uint32_t frame = Read(object + kStateFrame);
    uint32_t state = frame ? Read(frame + kFrameState) : 0;
    return state ? Read(state + kName) : 0;
}

// the controller's targeting and sniping states, by name as Double Agent's views.cpp has them; a state seen is
// looked up once
bool Aiming(uint32_t state) {
    static std::unordered_map<uint32_t, bool> known;
    auto it = known.find(state);
    if (it != known.end()) return it->second;
    std::string name = NameText(state);
    if (name.empty()) return false;
    for (char& c : name) c = char(std::tolower(uint8_t(c)));
    bool aiming = name.find("targeting") != std::string::npos || name.find("sniping") != std::string::npos;
    known.emplace(state, aiming);
    LOG_FIRST(200, "camera: controller state %s (%04X)%s", name.c_str(), state, aiming ? ", aiming" : "");
    return aiming;
}

void UpdateAiming(uint32_t controller) {
    static bool once = false;
    if (!once) {
        once = true;
        uint32_t cls = Read(controller + kClass);
        LOGI("camera: controller %08X of class %s", controller, NameText(cls ? Read(cls + kName) : 0).c_str());
    }
    input::SetAiming(Aiming(StateOf(controller)));
}

float g_yaw_rest = 0.0f;
float g_pitch_rest = 0.0f;

int32_t TakeUnits(float degrees, float& rest) {
    float total = degrees * kUnitsPerDegree + rest;
    float whole = std::trunc(total);
    rest = total - whole;
    return int32_t(whole);
}

void AddUnits(uint32_t address, int32_t units) { W32(address, R32(address) + uint32_t(units)); }

void ApplyGyro(uint32_t controller) {
    const app::Settings& s = app::Get();
    // taken every update all the same, so no turn gathered before it is on is let out when it starts
    gyro::Turn turn = gyro::Take(s.gyro);
    if (app::MenuOpen()) return;
    if (s.gyro.aim_only && !input::Aiming()) return;
    if (turn.recenter) W32(controller + kPitch, 0);
    // the gyro's yaw is left positive and its pitch down positive
    float yaw = s.invert_pad_x != s.gyro.invert_x ? turn.yaw : -turn.yaw;
    float pitch = s.invert_pad_y ? turn.pitch : -turn.pitch;
    AddUnits(controller + kYaw, TakeUnits(yaw, g_yaw_rest));
    AddUnits(controller + kPitch, TakeUnits(pitch, g_pitch_rest));
}

}  // namespace

// EPlayerCam's rotation update, thiscall (Double Agent's sub_000FD37E)
X86_EXTERN_FUNC(__imp__sub_000BCF8A);
X86_FUNC(sub_000BCF8A) {
    uint32_t cam = ctx.ecx;
    uint32_t controller = R32(cam + kController);
    if (!controller) {
        __imp__sub_000BCF8A(ctx, base);
        return;
    }
    LOG_FIRST(1, "camera: gyro and stick sensitivity on the player camera %08X", cam);
    UpdateAiming(controller);
    ApplyGyro(controller);
    uint32_t yaw = R32(cam + kYawSpeed);
    uint32_t pitch = R32(cam + kPitchSpeed);
    input::LookScale scale = input::PadLookScale(input::Aiming());
    W32(cam + kYawSpeed, std::bit_cast<uint32_t>(std::bit_cast<float>(yaw) * scale.x));
    W32(cam + kPitchSpeed, std::bit_cast<uint32_t>(std::bit_cast<float>(pitch) * scale.y));
    __imp__sub_000BCF8A(ctx, base);
    W32(cam + kYawSpeed, yaw);
    W32(cam + kPitchSpeed, pitch);
}
