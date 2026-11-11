// Gyro aiming and the stick's sensitivity, as Double Agent's (src/game/gyro_look.cpp, mouse_pitch.cpp): the gyro's
// turn is added to the player controller's rotation before the camera update reads it, so the camera's own pitch
// clamp holds it, and the stick turns by aTurn * yaw speed and aLookUp * pitch speed, which the sensitivities scale
// for the update. Chaos Theory's EPlayerCam is Double Agent's at other offsets; the sensitivities while aiming and
// the gyro only while aiming wait for its aiming state.
#include <bit>
#include <cmath>

#include "app/app.h"
#include "core/gyro.h"
#include "core/input.h"
#include "core/log.h"
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
    ApplyGyro(controller);
    uint32_t yaw = R32(cam + kYawSpeed);
    uint32_t pitch = R32(cam + kPitchSpeed);
    input::LookScale scale = input::PadLookScale(false);
    W32(cam + kYawSpeed, std::bit_cast<uint32_t>(std::bit_cast<float>(yaw) * scale.x));
    W32(cam + kPitchSpeed, std::bit_cast<uint32_t>(std::bit_cast<float>(pitch) * scale.y));
    __imp__sub_000BCF8A(ctx, base);
    W32(cam + kYawSpeed, yaw);
    W32(cam + kPitchSpeed, pitch);
}
