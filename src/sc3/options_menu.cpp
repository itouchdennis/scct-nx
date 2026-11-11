// RECOMP SETTINGS and CONTROLLER under Online in Chaos Theory's options menu, as Double Agent's
// (src/game/options_menu.cpp): each lists its sections (the panel's headings) in P_Options' own list, a section opens
// the Video page (P_Video) with the panel's rows in its list until B. A opens the list of a row's values (an on / off
// row flips) or rebinds, Left and Right step. Both games build their menus on Magma; Chaos Theory's is an older one,
// its lists have one column, so a row's text is the label and the value, and a change rebuilds the list.
#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/app.h"
#include "app/translations.h"
#include "core/input.h"
#include "core/log.h"
#include "xdk/hle.h"

using namespace kernel;

namespace dispatch {
uint32_t HostThunk(X86Func* fn);
}  // namespace dispatch

namespace {

// COptionsMenu: +8 the listener its OnPress, OnBack and OnKey get, +0x14 owner, +0x24 P_Options, +0x28 its list,
// +0x40 P_Video, +0x44 its list, +0x78 the Video options. Owner +0x14 is set when a key was taken.
constexpr uint32_t kListener = 8;
constexpr uint32_t kOwner = 0x14;
constexpr uint32_t kOptionsPage = 0x24;
constexpr uint32_t kOptionsList = 0x28;
constexpr uint32_t kVideo = 0x40;
constexpr uint32_t kVideoList = 0x44;
constexpr uint32_t kTaken = 0x14;
constexpr uint32_t kOptionsName = 0x4BE7B4;  // "P_Options"
constexpr uint32_t kVideoName = 0x4BE78C;    // "P_Video"
constexpr uint32_t kListName = 0x4BE830;     // "Lst_Options"
constexpr uint32_t kPopupName = 0x4DA068;    // "P_OptionPopup"
constexpr uint32_t kTitleName = 0x4BC3D0;    // "L_Title"
constexpr uint32_t kTopName = 0x4BC3E8;      // "L_Text", the upper title
// cdecl (page, element, section, key, package): the element's text from the localization, as the menu sets them
constexpr uint32_t kSetTitle = 0x1454EF;
constexpr uint32_t kSection = 0x4BE884;      // "OPTIONS"
constexpr uint32_t kVideoTitle = 0x4BE86C;   // "VideoTitle"
constexpr uint32_t kOptionsTitle = 0x4BEC3C; // "OptionsTitle"
constexpr uint32_t kPackage = 0x4BBDC4;      // "Localization\PreGameMenus"
constexpr uint32_t kIsPage = 0x2159DD;       // thiscall owner (name): that page has the screen
constexpr uint32_t kIsElement = 0x14534A;    // cdecl (instance, name)
constexpr uint32_t kFind = 0x12D640;         // cdecl (container, name): instance, its element at +8
constexpr uint32_t kFillVideo = 0x143CCD;    // thiscall menu
constexpr uint32_t kResetFocus = 0x24595D;   // cdecl (page)
constexpr uint32_t kShow = 0x2158AA;         // thiscall owner (page, show, 1, 0)
constexpr uint32_t kSetText = 0x130C11;      // thiscall FString (text)
constexpr uint32_t kSoundPlayer = 0x130EE4;  // thiscall owner
constexpr uint32_t kSoundOwner = 0x12BFF8;
// the player's vtable +0x7C plays (sound, owner +4's +0x58, 3, 0, 0, 0, 0), the sound at +0x2B4 of the sound owner
constexpr uint32_t kPlay = 0x7C;
constexpr uint32_t kSelectSound = 0x2B4;
constexpr uint32_t kMenuPlayer = 0x04;
constexpr uint32_t kPlayerSlot = 0x58;
// the stock option popup, through the Video options and their popup controller at +0x18. Open (thiscall options,
// FString array, row, option, ret 0xC) lists the array; the option's vtable is called back, +0 with the row picked
// and +4 on B (thiscall row, item, FString*, ret 0xC), +8 as the selection moves
constexpr uint32_t kVideoOptions = 0x78;
constexpr uint32_t kPopupController = 0x18;
constexpr uint32_t kOpenPopup = 0x15DF3E;
constexpr uint32_t kOnCancel = 4;
constexpr uint32_t kPopupList = 0x80 + 8;  // controller: +0x80 the popup, its +8 list
constexpr uint32_t kPopupSize = 0x4000;
constexpr uint32_t kMaxChoices = 128;
// a stock option's vtable (the Video page's first), its +8 does nothing
constexpr uint32_t kOptionVtable = 0x4BE6F4;
constexpr uint32_t kOptionSlots = 14;
// list: thiscall Clear, AddItem(text, 0) (the row), SetCell(row, column, text), Layout, Select(row, 0, 1); +0x50 its
// items (thiscall count), +0x60 its columns (thiscall count), +0x8E rows shown, +0x90 selection. A list with columns
// draws its rows' cells, each row must have them (the Video list: name and value)
constexpr uint32_t kClear = 0x248930;
constexpr uint32_t kAddItem = 0x248B9E;
constexpr uint32_t kSetCell = 0x248192;
constexpr uint32_t kColumns = 0x1C396B;
constexpr uint32_t kColumnsPart = 0x60;
constexpr uint32_t kEmpty = 0x51A6D8;  // the game's empty wide string
constexpr size_t kRowsShownMax = 7;  // ours at once, the list scrolls past them
constexpr uint32_t kLayout = 0x246533;
constexpr uint32_t kSelect = 0x246C35;
constexpr uint32_t kCount = 0x1313B6;
constexpr uint32_t kItems = 0x50;
constexpr uint32_t kRowsShown = 0x8E;
constexpr uint32_t kSelection = 0x90;
// instance: +8 its widget; a widget's vtable +0xC its type, its text an FString at +0xC (a text, type 8) or +0x24
// (buttons, 2 to 4), as kSetTitle has it
constexpr uint32_t kInstanceWidget = 8;
constexpr uint32_t kGetType = 0xC;
constexpr uint32_t kTextType = 8;
constexpr uint32_t kFirstButtonType = 2;
constexpr uint32_t kLastButtonType = 4;
// virtual key codes
constexpr uint32_t kLeft = 0x25;
constexpr uint32_t kRight = 0x27;
constexpr uint32_t kEscape = 0x1B;
// short enough for one line of the options list in each language (the German "Recomp-Einstellungen" took two)
const char* const kCaptions[] = {"RECOMP OPTIONS", "MOUSE AND KEYBOARD", "CONTROLLER"};
constexpr int kListedModes[] = {0, 2};  // no mouse and keyboard rows for Chaos Theory yet
constexpr int kCaptionCount = int(std::size(kListedModes));
const std::vector<std::string_view> kSections[] = {
    {"DISPLAY", "GRAPHICS", "GENERAL"},
    {"MOUSE", "KEYBOARD"},
    {"GENERAL", "GYRO", "BUTTONS"},
};
static_assert(std::size(kSections) == std::size(kCaptions));

// in the game's language, in capitals as the menu's own entries
std::u16string Text(std::string_view text) {
    std::u16string t = app::Translate(text, [](std::string_view s) { return std::string(s); });
    for (char16_t& c : t)
        if ((c >= u'a' && c <= u'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7)) c = char16_t(c - 0x20);
    return t;
}

bool Listed(const app::rows::Row& r) {
    using app::rows::Kind;
    if (r.kind == Kind::Value) return app::rows::Shown(r);
    return r.kind == Kind::Bind || r.kind == Kind::PadBind;
}

std::u16string Name(std::string_view text) { return std::u16string(text.begin(), text.end()); }

struct Take {
    int mode = -1;
    uint32_t menu = 0;
    uint32_t owner = 0;
    uint32_t page = 0;
    uint32_t list = 0;
    bool leaving = false;
    bool seen = false;  // its page has had the screen
    std::vector<const app::rows::Row*> rows;  // per list row, null restores defaults
    int section = -1;
    bool refill = false;
    int last_row = 0;
};
Take g_take;  // game thread

const app::rows::Row kGap{app::rows::Kind::Heading, -1, ""};
int g_first = -1;     // P_Options row of RECOMP SETTINGS
uint8_t g_shown = 0;  // P_Video's own rows shown
bool g_titled = false;  // P_Video's titles are ours
bool g_restore = false;
int g_capture = -1;
bool g_armed = false;
uint32_t g_text = 0;
uint32_t g_popup = 0;  // our option (+0) and its vtable (+0x20), the array (+0x60), its strings, their text
constexpr uint32_t kPopupVtable = 0x20;
constexpr uint32_t kPopupArray = 0x60;
int g_popup_row = -1;
int g_popup_current = 0;
constexpr uint32_t kWideSize = 0x400;
constexpr uint32_t kMaxWideChars = 0x1FF;

uint32_t Wide16(const std::u16string& text) {
    if (!g_text) g_text = mem::AllocSystem(kWideSize);
    size_t n = std::min<size_t>(text.size(), kMaxWideChars);
    for (size_t i = 0; i < n; i++) W16(g_text + uint32_t(i) * 2, uint16_t(text[i]));
    W16(g_text + uint32_t(n) * 2, 0);
    return g_text;
}

int Count(X86Context& ctx, uint32_t list) {
    ctx.ecx = list + kItems;
    return int(int16_t(CallGuest(ctx, kCount, {}) & 0xFFFF));
}

bool IsPage(X86Context& ctx, uint32_t owner, uint32_t name) {
    ctx.ecx = owner;
    return CallGuest(ctx, kIsPage, {name}) & 0xFF;
}

void Sound(X86Context& ctx, uint32_t owner) {
    ctx.ecx = owner;
    uint32_t player = CallGuest(ctx, kSoundPlayer, {});
    if (!player) return;
    uint32_t sounds = CallGuest(ctx, kSoundOwner, {});
    ctx.ecx = player;
    CallGuest(ctx, R32(R32(player) + kPlay),
              {R32(sounds + kSelectSound), R32(R32(owner + kMenuPlayer) + kPlayerSlot), 3, 0, 0, 0, 0});
}

void Unblock() { input::SetBlocked(app::MenuOpen() || app::ConsoleOpen()); }

// a row's label and value
std::pair<std::u16string, std::u16string> RowText(const app::rows::Row* r, int index) {
    if (r == &kGap) return {};
    if (!r) return {Text("RESTORE DEFAULTS"), {}};
    const char* prompt = r->kind == app::rows::Kind::PadBind ? "PRESS A BUTTON" : "PRESS A KEY";
    std::u16string value;
    if (g_capture == index) value = Text(prompt);
    else {
        std::string v = app::rows::Value(*r);
        value = r->kind == app::rows::Kind::Value || v == "UNBOUND" ? Text(v) : Name(v);
    }
    return {Text(r->label), value};
}

// the list built again with the rows' text, the selection kept: as the stock fill, an empty row and its cells
void Fill(X86Context& ctx, int select) {
    uint32_t list = g_take.list;
    ctx.ecx = list + kColumnsPart;
    int columns = int(int8_t(CallGuest(ctx, kColumns, {}) & 0xFF));
    ctx.ecx = list;
    CallGuest(ctx, kClear, {});
    for (size_t i = 0; i < g_take.rows.size(); i++) {
        auto [label, value] = RowText(g_take.rows[i], int(i));
        std::u16string whole = value.empty() ? label : label + u"  " + value;
        ctx.ecx = list;
        uint32_t row = CallGuest(ctx, kAddItem, {columns > 0 ? kEmpty : Wide16(whole), 0}) & 0xFFFF;
        if (columns <= 0) continue;
        ctx.ecx = list;
        CallGuest(ctx, kSetCell, {row, 0, Wide16(columns > 1 ? label : whole)});
        if (columns > 1) {
            ctx.ecx = list;
            CallGuest(ctx, kSetCell, {row, 1, Wide16(value)});
        }
    }
    // no more than the stock rows: the list draws that many, more reach past what it has
    G<uint8_t>(list + kRowsShown) = uint8_t(std::min<size_t>(g_take.rows.size(), kRowsShownMax));
    ctx.ecx = list;
    CallGuest(ctx, kLayout, {});
    ctx.ecx = list;
    CallGuest(ctx, kSelect, {uint32_t(std::clamp(select, 0, std::max(0, int(g_take.rows.size()) - 1))), 0, 1});
}

void Rows() {
    using app::rows::Kind;
    size_t count = 0;
    const app::rows::Row* page = app::rows::Page(g_take.mode, &count);
    g_take.rows.clear();
    for (size_t i = size_t(g_take.section) + 1; i < count && page[i].kind != Kind::Heading; i++)
        if (Listed(page[i])) g_take.rows.push_back(&page[i]);
    if (g_take.mode >= 1) {
        g_take.rows.push_back(&kGap);
        g_take.rows.push_back(nullptr);
    }
}

// the stock option: +4 its options, +0xC its value
constexpr uint32_t kOptionOptions = 4;
constexpr uint32_t kOptionValue = 0xC;
// FString, and the array of them: +0 data, +4 count, +8 capacity
constexpr uint32_t kFStringCount = 4;
constexpr uint32_t kFStringCapacity = 8;
constexpr uint32_t kFStringSize = 0xC;

void Picked(X86Context& __restrict ctx, uint8_t*) {
    int pick = int(R32(ctx.esp + 4));
    const app::rows::Row* r = g_take.mode >= 0 && g_popup_row >= 0 && g_popup_row < int(g_take.rows.size())
                                  ? g_take.rows[size_t(g_popup_row)] : nullptr;
    if (r && pick != g_popup_current) {
        app::rows::Choose(*r, pick);
        g_take.refill = true;
    }
    g_popup_row = -1;
    ctx.esp += 16;  // ret 0xC
}

void Cancelled(X86Context& __restrict ctx, uint8_t*) {
    g_popup_row = -1;
    ctx.esp += 16;  // ret 0xC
}

uint32_t WriteChoices(uint32_t strings, const std::vector<std::string>& choices) {
    uint32_t text = strings + kMaxChoices * kFStringSize;
    uint32_t n = 0;
    for (const std::string& choice : choices) {
        std::u16string t = Text(choice);
        uint32_t length = uint32_t(t.size()) + 1;  // FString counts its terminator
        if (n == kMaxChoices || text + length * 2 > g_popup + kPopupSize) break;
        for (uint32_t i = 0; i < length; i++) W16(text + i * 2, i < t.size() ? uint16_t(t[i]) : 0);
        W32(strings + n * kFStringSize, text);
        W32(strings + n * kFStringSize + kFStringCount, length);
        W32(strings + n * kFStringSize + kFStringCapacity, length);
        text += length * 2;
        n++;
    }
    return n;
}

void Popup(X86Context& ctx, int row, const std::vector<std::string>& choices, int current) {
    if (!R32(g_take.menu + kVideoOptions + kPopupController)) {
        LOGI("options menu: no popup controller");
        return;
    }
    if (!g_popup) {
        g_popup = mem::AllocSystem(kPopupSize);
        uint32_t vtable = g_popup + kPopupVtable;
        for (uint32_t i = 0; i < kOptionSlots; i++) W32(vtable + i * 4, R32(kOptionVtable + i * 4));
        W32(vtable, dispatch::HostThunk(Picked));
        W32(vtable + kOnCancel, dispatch::HostThunk(Cancelled));
        W32(g_popup, vtable);
    }
    W32(g_popup + kOptionOptions, g_take.menu + kVideoOptions);
    W32(g_popup + kOptionValue, uint32_t(current));
    uint32_t array = g_popup + kPopupArray;
    uint32_t strings = array + kFStringSize;
    uint32_t n = WriteChoices(strings, choices);
    W32(array, strings);
    W32(array + kFStringCount, n);
    W32(array + kFStringCapacity, n);
    g_popup_row = row;
    g_popup_current = current;
    ctx.ecx = g_take.menu + kVideoOptions;
    CallGuest(ctx, kOpenPopup, {array, uint32_t(current), g_popup});
    uint32_t list = R32(R32(g_take.menu + kVideoOptions + kPopupController) + kPopupList);
    if (list) {
        ctx.ecx = list;
        CallGuest(ctx, kSelect, {uint32_t(current), 0, 1});
    }
    LOGI("options menu: popup of %u for row %d", n, row);
}

// a page element's text, as kSetTitle sets it
void SetText(X86Context& ctx, uint32_t page, uint32_t name, const std::u16string& text) {
    uint32_t instance = CallGuest(ctx, kFind, {page, name});
    uint32_t widget = instance ? R32(instance + kInstanceWidget) : 0;
    if (!widget) return;
    ctx.ecx = widget;
    uint32_t type = CallGuest(ctx, R32(R32(widget) + kGetType), {});
    if (type != kTextType && (type < kFirstButtonType || type > kLastButtonType)) return;
    ctx.ecx = widget + (type == kTextType ? 0xC : 0x24);
    CallGuest(ctx, kSetText, {Wide16(text)});
}

void RestoreTitles(X86Context& ctx, uint32_t page) {
    if (!g_titled || !page) return;
    CallGuest(ctx, kSetTitle, {page, kTitleName, kSection, kVideoTitle, kPackage});
    CallGuest(ctx, kSetTitle, {page, kTopName, kSection, kOptionsTitle, kPackage});
    g_titled = false;
}

void Enter(X86Context& ctx, uint32_t menu, int mode, int section) {
    uint32_t owner = R32(menu + kOwner);
    uint32_t page = R32(menu + kVideo);
    uint32_t list = R32(menu + kVideoList);
    if (!owner || !page || !list || section < 0) return;
    if (!g_shown) g_shown = G<uint8_t>(list + kRowsShown);
    LOGI("options menu: the Video list shows %u rows", g_shown);
    g_take = {mode, menu, owner, page, list, false, false, {}, section};
    Rows();
    Fill(ctx, 0);
    size_t count = 0;
    const app::rows::Row* rows = app::rows::Page(mode, &count);
    SetText(ctx, page, kTitleName, Text(rows[section].label));
    SetText(ctx, page, kTopName, Text(kCaptions[mode]));
    g_titled = true;
    CallGuest(ctx, kResetFocus, {page});
    ctx.ecx = owner;
    CallGuest(ctx, kShow, {page, 1, 1, 0});
    Sound(ctx, owner);
    LOGI("options menu: %s, %zu rows", kCaptions[mode], g_take.rows.size());
}

void StopCapture() {
    if (g_capture < 0) return;
    g_capture = -1;
    Unblock();
}

void Leave(X86Context& ctx) {
    StopCapture();
    ctx.ecx = g_take.menu;
    CallGuest(ctx, kFillVideo, {});
    RestoreTitles(ctx, g_take.page);
    g_take = {};
}

// step 0 is A
void Act(X86Context& ctx, int step) {
    int row = int16_t(R16(g_take.list + kSelection));
    LOGI("options menu: row %d step %d", row, step);
    if (row < 0 || row >= int(g_take.rows.size())) return;
    const app::rows::Row* r = g_take.rows[size_t(row)];
    if (r == &kGap) return;
    if (!r) {
        if (step) return;
        app::rows::Defaults(g_take.mode);
    } else if (r->kind == app::rows::Kind::Value) {
        if (!app::rows::Enabled(*r)) return;
        int current = 0;
        std::vector<std::string> choices = app::rows::Choices(*r, &current);
        bool flip = choices.size() == 2 && choices[0] == "OFF" && choices[1] == "ON";
        if (choices.size() > 1 && (!flip || step)) {
            if (!step) {
                Popup(ctx, row, choices, current);
                return;
            }
        }
        app::rows::Step(*r, step ? step : 1);
    } else {
        if (step) return;
        g_capture = row;
        g_armed = false;
        input::SetBlocked(true);
    }
    g_take.refill = true;
    Sound(ctx, g_take.owner);
}

bool OurPage(X86Context& ctx, uint32_t menu) {
    return g_take.mode >= 0 && !g_take.leaving && menu == g_take.menu && IsPage(ctx, g_take.owner, kVideoName) &&
           !IsPage(ctx, g_take.owner, kPopupName);
}

// RECOMP SETTINGS and CONTROLLER list their sections in P_Options' own list
struct Sections {
    bool on = false;
    int mode = 0;
    int pending = 0;  // 1 shows them, -1 brings the options back, once the list's key handling is over
    uint32_t menu = 0;
    uint32_t page = 0;
    uint32_t list = 0;
};
Sections g_sections;  // game thread

std::vector<int> ListedSections(int mode) {
    size_t count = 0;
    const app::rows::Row* rows = app::rows::Page(mode, &count);
    std::vector<int> out;
    for (std::string_view name : kSections[mode]) {
        for (size_t i = 0; i < count; i++) {
            if (rows[i].kind != app::rows::Kind::Heading || rows[i].label != name) continue;
            bool any = false;
            for (size_t j = i + 1; !any && j < count && rows[j].kind != app::rows::Kind::Heading; j++)
                any = Listed(rows[j]);
            if (any) out.push_back(int(i));
            break;
        }
    }
    return out;
}

bool OnSections(X86Context& ctx, uint32_t menu) {
    return g_sections.on && g_take.mode < 0 && menu == g_sections.menu &&
           IsPage(ctx, R32(menu + kOwner), kOptionsName) && !IsPage(ctx, R32(menu + kOwner), kPopupName);
}

void Replay(X86Context& ctx, int row) {
    Sections& s = g_sections;
    uint32_t owner = R32(s.menu + kOwner);
    ctx.ecx = owner;
    CallGuest(ctx, kShow, {s.page, 0, 1, 0});
    CallGuest(ctx, kResetFocus, {s.page});
    ctx.ecx = owner;
    CallGuest(ctx, kShow, {s.page, 1, 1, 0});
    ctx.ecx = s.list;
    CallGuest(ctx, kSelect, {uint32_t(row), 0, 1});
}

void SectionsTick(X86Context& ctx) {
    Sections& s = g_sections;
    int pending = s.pending;
    s.pending = 0;
    if (pending > 0 && s.list) {
        s.on = true;
        ctx.ecx = s.list;
        CallGuest(ctx, kClear, {});
        size_t count = 0;
        const app::rows::Row* rows = app::rows::Page(s.mode, &count);
        std::vector<int> sections = ListedSections(s.mode);
        for (int heading : sections) {
            ctx.ecx = s.list;
            CallGuest(ctx, kAddItem, {Wide16(Text(rows[heading].label)), 0});
        }
        G<uint8_t>(s.list + kRowsShown) = uint8_t(sections.size());
        ctx.ecx = s.list;
        CallGuest(ctx, kLayout, {});
        Replay(ctx, 0);
        LOGI("options menu: %zu sections of %s", sections.size(), kCaptions[s.mode]);
    } else if (pending < 0) {
        s.on = false;
        ctx.ecx = s.menu;
        CallGuest(ctx, 0x143BA6, {});  // the options list, with our captions
        Replay(ctx, g_first);
    }
}

void PassGap(X86Context& ctx) {
    int row = int16_t(R16(g_take.list + kSelection));
    int rows = int(g_take.rows.size());
    if (row >= 0 && row < rows && g_take.rows[size_t(row)] == &kGap) {
        row = std::clamp(row + (g_take.last_row < row ? 1 : -1), 0, rows - 1);
        ctx.ecx = g_take.list;
        CallGuest(ctx, kSelect, {uint32_t(row), 0, 1});
    }
    g_take.last_row = row;
}

void CaptureTick() {
    const app::rows::Row* r = g_take.rows[size_t(g_capture)];
    bool pad = r && r->kind == app::rows::Kind::PadBind;
    int vk = input::PressedKey();
    int button = pad ? input::PressedPadButton() : -1;
    if (!g_armed) {
        g_armed = !vk && button < 0;
        return;
    }
    if (button >= 0) input::BindPad(size_t(r->id), button);
    else if (vk && !pad && vk != kEscape && r) input::Bind(size_t(r->id), vk);
    else if (vk != kEscape) return;
    StopCapture();
    g_take.refill = true;
}

// game thread, every input update
void MenuTick(X86Context& ctx) {
    SectionsTick(ctx);
    if (g_take.mode < 0) return;
    if (g_take.leaving) {
        Leave(ctx);
        LOGI("options menu: stock rows back");
        return;
    }
    static uint32_t revision = 0;
    if (uint32_t r = app::SettingsRevision(); r != revision && g_capture < 0 && g_popup_row < 0) {
        revision = r;
        g_take.refill = true;
    }
    if (g_capture >= 0) CaptureTick();
    if (g_take.refill && g_popup_row < 0) {
        g_take.refill = false;
        Fill(ctx, std::max(0, int(int16_t(R16(g_take.list + kSelection)))));
    }
    // the pause menu closed under us: the stock rows come back with the next options page (its fill), the title
    // with the options list
    bool shown = IsPage(ctx, g_take.owner, kVideoName) || IsPage(ctx, g_take.owner, kPopupName);
    if (shown) {
        g_take.seen = true;
    } else if (g_take.seen) {
        StopCapture();
        g_take = {};
        g_sections.on = false;
        g_restore = true;
        return;
    }
    PassGap(ctx);
}

}  // namespace

// the viewport's input update (vtable slot, as Double Agent's sub_0004B525): our tick after it, below a scratch block
X86_EXTERN_FUNC(__imp__sub_0002449E);
X86_FUNC(sub_0002449E) {
    __imp__sub_0002449E(ctx, base);
    uint32_t esp = ctx.esp;
    ctx.esp = ((esp - 0x40) & ~0xFu) - 0x10;
    uint32_t eax = ctx.eax;
    MenuTick(ctx);
    ctx.eax = eax;
    ctx.esp = esp;
}

// options list fill (thiscall menu): Controller, Video, Sound, Online, then ours
X86_EXTERN_FUNC(__imp__sub_00143BA6);
X86_FUNC(sub_00143BA6) {
    uint32_t menu = ctx.ecx;
    g_sections.on = false;
    __imp__sub_00143BA6(ctx, base);
    uint32_t list = R32(menu + kOptionsList);
    int count = list ? Count(ctx, list) : 0;
    if (count < 2) return;
    g_first = count;
    for (int mode : kListedModes) {
        ctx.ecx = list;
        CallGuest(ctx, kAddItem, {Wide16(Text(kCaptions[mode])), 0});
    }
    G<uint8_t>(list + kRowsShown) = uint8_t(count + kCaptionCount);
    ctx.ecx = list;
    CallGuest(ctx, kLayout, {});
    if (g_restore) RestoreTitles(ctx, R32(menu + kVideo));
    g_restore = false;
}

// OnPress (thiscall listener, instance, ret 4)
X86_EXTERN_FUNC(__imp__sub_00143E19);
X86_FUNC(sub_00143E19) {
    uint32_t menu = ctx.ecx - kListener;
    uint32_t instance = R32(ctx.esp + 4);
    uint32_t owner = R32(menu + kOwner);
    uint32_t list = R32(menu + kOptionsList);
    bool ours_list = instance && (CallGuest(ctx, kIsElement, {instance, kListName}) & 0xFF);
    int row = ours_list ? int16_t(R16(R32(instance + kInstanceWidget) + kSelection)) : -1;
    auto taken = [&](uint32_t result) {
        G<uint8_t>(owner + kTaken) = 1;
        ctx.eax = result;
        ctx.esp += 8;
    };
    if (ours_list && R32(instance + kInstanceWidget) == list && OnSections(ctx, menu)) {
        std::vector<int> sections = ListedSections(g_sections.mode);
        if (row >= 0 && row < int(sections.size())) Enter(ctx, menu, g_sections.mode, sections[size_t(row)]);
        return taken(0);
    }
    if (ours_list && g_take.mode < 0 && g_first >= 0 && R32(instance + kInstanceWidget) == list && row >= g_first &&
        row < g_first + kCaptionCount && IsPage(ctx, owner, kOptionsName)) {
        Sections& s = g_sections;
        s.pending = 1;
        s.mode = kListedModes[row - g_first];
        s.menu = menu;
        s.page = R32(menu + kOptionsPage);
        s.list = list;
        Sound(ctx, owner);
        return taken(0);
    }
    if (ours_list && OurPage(ctx, menu)) {
        if (g_capture < 0) Act(ctx, 0);
        return taken(1);
    }
    ctx.ecx = menu + kListener;
    __imp__sub_00143E19(ctx, base);
}

// OnBack (thiscall listener, one argument, ret 4)
X86_EXTERN_FUNC(__imp__sub_001443FE);
X86_FUNC(sub_001443FE) {
    uint32_t menu = ctx.ecx - kListener;
    if (OurPage(ctx, menu)) {
        LOGI("options menu: back");
        uint32_t owner = g_take.owner;
        StopCapture();
        g_take.leaving = true;
        ctx.ecx = owner;
        CallGuest(ctx, kShow, {g_take.page, 0, 1, 0});
        G<uint8_t>(owner + kTaken) = 0;
        ctx.eax = 0;
        ctx.esp += 8;
        return;
    }
    if (OnSections(ctx, menu)) {
        g_sections.pending = -1;
        Sound(ctx, R32(menu + kOwner));
        G<uint8_t>(R32(menu + kOwner) + kTaken) = 0;
        ctx.eax = 0;
        ctx.esp += 8;
        return;
    }
    ctx.ecx = menu + kListener;
    __imp__sub_001443FE(ctx, base);
}

// OnKey (thiscall listener, instance, int* key, ret 8): arrow key codes
X86_EXTERN_FUNC(__imp__sub_001441E5);
X86_FUNC(sub_001441E5) {
    uint32_t menu = ctx.ecx - kListener;
    uint32_t key_at = R32(ctx.esp + 8);
    uint32_t key = key_at ? R32(key_at) : 0;
    if ((key == kLeft || key == kRight) && OurPage(ctx, menu)) {
        if (g_capture < 0) Act(ctx, key == kLeft ? -1 : 1);
        G<uint8_t>(g_take.owner + kTaken) = 1;
        ctx.eax = 1;
        ctx.esp += 12;
        return;
    }
    ctx.ecx = menu + kListener;
    __imp__sub_001441E5(ctx, base);
}
