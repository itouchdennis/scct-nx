// Gyro aiming and the stick's sensitivity, as Double Agent's (src/game/gyro_look.cpp, mouse_pitch.cpp): the gyro's
// turn is added to the player controller's rotation before the camera update reads it, so the camera's own pitch
// clamp holds it, and the stick turns by aTurn * yaw speed and aLookUp * pitch speed, which the sensitivities scale
// for the update. Chaos Theory's EPlayerCam is Double Agent's at other offsets. Aiming (the gun raised, the sniper
// scope) is the controller's script state, as Double Agent's views.cpp reads it.
#include <algorithm>
#include <bit>
#include <cmath>
#include <iterator>

#include "app/app.h"
#include "core/gyro.h"
#include "core/input.h"
#include "core/log.h"
#include "game/unreal.h"
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

// object: +0xC state frame, +0x20 name, +0x24 class; state frame: +0x1C state, as Double Agent's (unreal.cpp)
constexpr uint32_t kStateFrame = 0x0C, kName = 0x20, kClass = 0x24, kFrameState = 0x1C;

// a read of guest memory that may not be there: 0 then
uint32_t Read(uint32_t address) { return address >= 0x10000 && mem::IsCommitted(address) ? R32(address) : 0; }

uint32_t StateOf(uint32_t object) {
    uint32_t frame = Read(object + kStateFrame);
    uint32_t state = frame ? Read(frame + kFrameState) : 0;
    return state ? Read(state + kName) : 0;
}

// the controller's targeting and sniping states, Double Agent's names
bool Aiming(uint32_t state) {
    using game::unreal::Name;
    static constexpr uint32_t kAiming[] = {
        Name("s_FirstPersonTargeting"),  Name("s_Targeting"),          Name("s_RappellingTargeting"),
        Name("s_DualRappellingTargeting"), Name("s_SplitTargeting"),   Name("s_HOHTargeting"),
        Name("s_GrabTargeting"),         Name("s_OverTrainTargeting"), Name("s_PlayerSniping"),
        Name("s_RappellingSniping"),     Name("s_DualRappellingSniping"), Name("s_SplitSniping"),
        Name("s_OverTrainSniping"),      Name("s_PlayerWalkingTargeting"), Name("s_FenceTargeting"),
    };
    return std::find(std::begin(kAiming), std::end(kAiming), state) != std::end(kAiming);
}

// the state as it changes, for a log of what Chaos Theory names them
void UpdateAiming(uint32_t controller) {
    static uint32_t last = ~0u;
    uint32_t state = StateOf(controller);
    bool aiming = Aiming(state);
    input::SetAiming(aiming);
    if (state == last) return;
    last = state;
    if (static bool once = false; !once) {
        once = true;
        uint32_t cls = Read(controller + kClass);
        LOGI("camera: controller %08X, class name %08X (EPlayerController %08X)", controller, cls ? Read(cls + kName) : 0,
             game::unreal::Name("EPlayerController"));
    }
    LOG_FIRST(200, "camera: controller state %08X%s", state, aiming ? ", aiming" : "");
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
