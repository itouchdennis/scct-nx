#pragma once
// The settings panel (app/menu.cpp, F10 on Windows) and the frame rate as an overlay the port's renderer draws over
// the game: ImGui's context, its input from the controllers and the keyboard, and the frame it builds. On a
// controller L + R + D-pad right opens and closes it, B closes it too; F10 on a keyboard.
#include <cstdint>

struct ImDrawData;

namespace port {

// main thread, before the game boots
void OverlayInit();
// main thread, after port::UpdatePads: the controllers' combination that opens or closes the panel
void OverlayPads();
// main thread: a key for the panel's keyboard navigation (an SDL scancode), down or up
void OverlayKey(int scancode, bool down);
// render thread, once a present: the overlay's frame at the output's size, null when nothing is shown
ImDrawData* OverlayFrame(uint32_t width, uint32_t height);

}  // namespace port
