#include "pbvp/ui_bridge.hpp"

#include "pbvp/log.hpp"

#include "nvse/GameTiles.h"

#include <Windows.h>
#include <Xinput.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>

namespace pbvp {
namespace {

constexpr std::uintptr_t kTileMenuArrayPointer = 0x011F350Cu;
constexpr std::uintptr_t kMenuVisibilityArray = 0x011F308Fu;
constexpr std::uintptr_t kTileImageVtable = 0x0106F01Cu;
constexpr std::uintptr_t kTileShaderPropertyVtable = 0x010B9D28u;
constexpr std::uintptr_t kNiSourceTextureVtable = 0x0109B9ECu;
constexpr std::uintptr_t kNiDx9SourceTextureDataVtable = 0x010ED37Cu;
constexpr std::uintptr_t kTileSetStringValueAddress = 0x00A01350u;
constexpr std::uintptr_t kTileSetFloatValueAddress = 0x00A012D0u;
constexpr std::uint32_t kMenuTypeMin = 0x3E9u;
constexpr std::uint32_t kMapMenuType = 0x3FFu;
constexpr std::uint32_t kValueX = ::Tile::kTileValue_x;
constexpr std::uint32_t kValueY = ::Tile::kTileValue_y;
constexpr std::uint32_t kValueVisible = ::Tile::kTileValue_visible;
constexpr std::uint32_t kValueHeight = ::Tile::kTileValue_height;
constexpr std::uint32_t kValueWidth = ::Tile::kTileValue_width;
constexpr std::uint32_t kValueString = ::Tile::kTileValue_string;
constexpr std::uint32_t kValueSystemColor = ::Tile::kTileValue_systemcolor;
constexpr std::size_t kMaxTileValues = 4096;
constexpr std::size_t kMaxTilesVisited = 512;
constexpr std::size_t kMaxParentDepth = 64;
constexpr std::size_t kMenuVirtualFunctionCount = 15u;
constexpr std::size_t kMenuHandleClickSlot = 3u;
constexpr std::size_t kMenuHandleKeyboardInputSlot = 12u;
constexpr std::uint32_t kOpenButtonId = 9100u;
constexpr std::uint32_t kBackButtonId = 9101u;
constexpr std::uint32_t kFirstCatalogRowId = 9110u;
constexpr std::uint32_t kLastCatalogRowId = 9117u;
constexpr std::uint32_t kPauseButtonId = 9120u;
constexpr std::uint32_t kStopButtonId = 9121u;
constexpr std::uint32_t kSeekBackButtonId = 9122u;
constexpr std::uint32_t kSeekForwardButtonId = 9123u;
constexpr std::uint32_t kPresentationButtonId = 9124u;
constexpr std::size_t kNvseKeyboardKeys = 256u;
constexpr std::size_t kNvseMouseButtonOffset = 256u;
constexpr std::size_t kNvseMouseWheelOffset = kNvseMouseButtonOffset + 8u;
constexpr std::size_t kNvseInputCount = kNvseMouseWheelOffset + 2u;
constexpr std::size_t kMouseRight = kNvseMouseButtonOffset + 1u;
constexpr std::size_t kMouseWheelUp = kNvseMouseWheelOffset;
constexpr std::size_t kMouseWheelDown = kNvseMouseWheelOffset + 1u;

enum class ResolveStatus : std::uint32_t {
    kMapHidden = 1u,
    kMenuArrayUnavailable,
    kMenuRootUnavailable,
    kVideoRectUnavailable,
    kVideoSizeUnavailable,
    kCanvasExtentUnavailable,
    kParentChainInvalid,
    kGeometryInvalid,
    kAccessViolation,
    kResolved,
};

struct Tile;

struct ListNode {
    void* data;
    ListNode* next;
};

struct ChildNode {
    ChildNode* next;
    ChildNode* previous;
    Tile* child;
};

struct TileValue {
    std::uint32_t id;
    Tile* parent;
    float number;
    char* string;
    void* action;
};

struct GameString {
    char* data;
    std::uint16_t length;
    std::uint16_t capacity;
};

struct Tile {
    void* vtable;
    ListNode children;
    std::uint32_t unknown_0c;
    void* values_vtable;
    TileValue** values;
    std::uint32_t value_count;
    std::uint32_t value_capacity;
    GameString name;
    Tile* parent;
    void* node;
    std::uint32_t flags;
    std::uint8_t unknown_34[4];
};

struct TileImage {
    Tile tile;
    float unknown_38;
    void* direct_texture;
    void* shader_property;
    std::uint8_t unknown_44[4];
};

struct TileMenuLayout {
    Tile tile;
    std::uint32_t unknown_38;
    void* menu;
};

struct MenuLayout {
    void** vtable;
    std::uint8_t unknown_04[0x1Cu];
    std::uint32_t id;
};

struct NvseKeyInfoLayout {
    std::uint8_t raw_state;
    std::uint8_t game_state;
    std::uint8_t inserted_state;
    std::uint8_t held;
    std::uint8_t tapped;
    std::uint8_t user_disabled;
    std::uint8_t script_disabled;
};

struct NvseInputStateLayout {
    std::array<NvseKeyInfoLayout, kNvseInputCount> keys;
};

struct TileShaderPropertyLayout {
    std::uint8_t unknown_00[0x60];
    void* source_texture;
    void* alpha_texture;
};

struct NiTextureLayout {
    std::uint8_t unknown_00[0x24];
    void* renderer_data;
};

struct NiDx9TextureDataLayout {
    std::uint8_t unknown_00[0x64];
    void* d3d_base_texture;
};

static_assert(sizeof(Tile) == 0x38);
static_assert(offsetof(Tile, values) == 0x14);
static_assert(offsetof(Tile, name) == 0x20);
static_assert(offsetof(Tile, parent) == 0x28);
static_assert(sizeof(TileImage) == 0x48);
static_assert(sizeof(TileMenuLayout) == 0x40);
static_assert(offsetof(TileMenuLayout, menu) == 0x3C);
static_assert(offsetof(MenuLayout, id) == 0x20);
static_assert(sizeof(NvseKeyInfoLayout) == 7u);
static_assert(offsetof(TileImage, direct_texture) == 0x3C);
static_assert(offsetof(TileImage, shader_property) == 0x40);
static_assert(offsetof(TileShaderPropertyLayout, source_texture) == 0x60);
static_assert(offsetof(TileShaderPropertyLayout, alpha_texture) == 0x64);
static_assert(offsetof(NiTextureLayout, renderer_data) == 0x24);
static_assert(offsetof(NiDx9TextureDataLayout, d3d_base_texture) == 0x64);

TileValue* FindValue(Tile* tile, std::uint32_t id) noexcept;
Tile* FindDescendant(Tile* root, const char* name) noexcept;

using MenuHandleClick = void(__thiscall*)(void*, std::uint32_t, Tile*);
using MenuHandleKeyboardInput = bool(__thiscall*)(void*, char);
using XInputGetStateFunction = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

std::array<void*, kMenuVirtualFunctionCount> g_map_menu_vtable{};
MenuHandleClick g_original_handle_click{};
MenuHandleKeyboardInput g_original_handle_keyboard{};
MenuLayout* g_hooked_map_menu{};
void** g_original_map_menu_vtable{};
std::atomic<std::uint32_t> g_pending_input_actions{0u};
std::atomic<bool> g_videos_page_active{false};
std::atomic<UiInputMethod> g_input_method{UiInputMethod::keyboard_mouse};
std::array<bool, kNvseInputCount> g_key_down{};
std::atomic<std::uintptr_t> g_nvse_input_state{};
InputSettings g_input_settings{};
HMODULE g_xinput_module{};
XInputGetStateFunction g_xinput_get_state{};
WORD g_previous_controller_buttons{};
bool g_controller_connected{};
POINT g_previous_cursor_position{};
bool g_cursor_position_known{};
bool g_input_hook_logged{};
bool g_input_hook_failure_logged{};
std::array<char, 192u> g_playback_prompt{};
std::array<char, 96u> g_catalog_prompt{};
std::array<char, 32u> g_catalog_back_prompt{};

void QueueAction(const UiInputAction action, const UiInputMethod method) noexcept {
    g_input_method.store(method, std::memory_order_release);
    g_pending_input_actions.fetch_or(
        static_cast<std::uint32_t>(action), std::memory_order_acq_rel);
}

UiInputAction ActionForButtonId(const std::uint32_t button_id) noexcept {
    switch (button_id) {
        case kOpenButtonId: return UiInputAction::open_page;
        case kBackButtonId: return UiInputAction::close_page;
        case kPauseButtonId: return UiInputAction::pause_resume;
        case kStopButtonId: return UiInputAction::stop;
        case kSeekBackButtonId: return UiInputAction::seek_backward;
        case kSeekForwardButtonId: return UiInputAction::seek_forward;
        case kPresentationButtonId: return UiInputAction::toggle_presentation;
        default:
            if (button_id >= kFirstCatalogRowId && button_id <= kLastCatalogRowId) {
                return static_cast<UiInputAction>(
                    static_cast<std::uint32_t>(UiInputAction::activate) |
                    ((button_id - kFirstCatalogRowId + 1u) << 16u));
            }
            return UiInputAction::none;
    }
}

void __fastcall MapMenuHandleClickHook(
    void* menu,
    void*,
    const std::uint32_t button_id,
    Tile* clicked_button) noexcept {
    const UiInputAction action = ActionForButtonId(button_id);
    if (action != UiInputAction::none &&
        (button_id == kOpenButtonId ||
         g_videos_page_active.load(std::memory_order_acquire))) {
        QueueAction(action, UiInputMethod::keyboard_mouse);
        return;
    }
    if (g_videos_page_active.load(std::memory_order_acquire)) {
        return;
    }
    if (g_original_handle_click != nullptr) {
        g_original_handle_click(menu, button_id, clicked_button);
    }
}

bool __fastcall MapMenuHandleKeyboardHook(
    void* menu,
    void*,
    const char input_character) noexcept {
    if (g_videos_page_active.load(std::memory_order_acquire)) {
        return true;
    }
    return g_original_handle_keyboard != nullptr
        ? g_original_handle_keyboard(menu, input_character)
        : false;
}

bool AddressInsideMainImage(const void* address) noexcept {
    if (address == nullptr) {
        return false;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (base == 0u) {
        return false;
    }
    __try {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
            return false;
        }
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
            base + static_cast<std::uintptr_t>(dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->OptionalHeader.SizeOfImage == 0u) {
            return false;
        }
        const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(address);
        return value >= base && value < base + nt->OptionalHeader.SizeOfImage;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

Tile* CurrentMapMenuRoot() noexcept {
    auto*** menu_array_pointer = reinterpret_cast<Tile***>(kTileMenuArrayPointer);
    if (menu_array_pointer == nullptr || *menu_array_pointer == nullptr) {
        return nullptr;
    }
    return (*menu_array_pointer)[kMapMenuType - kMenuTypeMin];
}

bool AttachMapMenuInput(Tile* menu_root) noexcept {
    if (menu_root == nullptr) {
        return false;
    }
    __try {
        auto* tile_menu = reinterpret_cast<TileMenuLayout*>(menu_root);
        auto* menu = static_cast<MenuLayout*>(tile_menu->menu);
        if (menu == nullptr || menu->id != kMapMenuType) {
            return false;
        }
        if (g_hooked_map_menu == menu && menu->vtable == g_map_menu_vtable.data()) {
            return true;
        }
        if (!AddressInsideMainImage(menu->vtable)) {
            return false;
        }
        for (std::size_t index = 0u; index < g_map_menu_vtable.size(); ++index) {
            if (!AddressInsideMainImage(menu->vtable[index])) {
                return false;
            }
            g_map_menu_vtable[index] = menu->vtable[index];
        }
        g_original_map_menu_vtable = menu->vtable;
        g_original_handle_click = reinterpret_cast<MenuHandleClick>(
            g_map_menu_vtable[kMenuHandleClickSlot]);
        g_original_handle_keyboard = reinterpret_cast<MenuHandleKeyboardInput>(
            g_map_menu_vtable[kMenuHandleKeyboardInputSlot]);
        g_map_menu_vtable[kMenuHandleClickSlot] =
            reinterpret_cast<void*>(&MapMenuHandleClickHook);
        g_map_menu_vtable[kMenuHandleKeyboardInputSlot] =
            reinterpret_cast<void*>(&MapMenuHandleKeyboardHook);
        menu->vtable = g_map_menu_vtable.data();
        g_hooked_map_menu = menu;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool GameInputPressedEdge(const std::size_t key) noexcept {
    if (key >= g_key_down.size()) {
        return false;
    }
    const std::uintptr_t address = g_nvse_input_state.load(std::memory_order_acquire);
    if (address == 0u) {
        return false;
    }
    __try {
        const auto* input = reinterpret_cast<const NvseInputStateLayout*>(address);
        const std::uint8_t value = input->keys[key].game_state;
        if (value > 1u) {
            return false;
        }
        const bool down = value != 0u;
        const bool pressed = down && !g_key_down[key];
        g_key_down[key] = down;
        return pressed;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_nvse_input_state.store(0u, std::memory_order_release);
        return false;
    }
}

void PollKeyboardAndMouse() noexcept {
    struct KeyAction final {
        std::size_t key;
        UiInputAction action;
    };
    const std::array<KeyAction, 11u> keys{{
        {g_input_settings.previous_item, UiInputAction::previous_item},
        {g_input_settings.next_item, UiInputAction::next_item},
        {g_input_settings.select_or_play, UiInputAction::activate},
        {g_input_settings.pause_resume, UiInputAction::pause_resume},
        {g_input_settings.back_or_stop, UiInputAction::close_page},
        {g_input_settings.seek_backward, UiInputAction::seek_backward},
        {g_input_settings.seek_forward, UiInputAction::seek_forward},
        {g_input_settings.toggle_color, UiInputAction::toggle_presentation},
        {kMouseRight, UiInputAction::close_page},
        {kMouseWheelUp, UiInputAction::previous_item},
        {kMouseWheelDown, UiInputAction::next_item},
    }};
    for (const KeyAction& key : keys) {
        if (GameInputPressedEdge(key.key)) {
            QueueAction(key.action, UiInputMethod::keyboard_mouse);
        }
    }
    POINT cursor{};
    if (GetCursorPos(&cursor) != FALSE) {
        if (g_cursor_position_known &&
            (cursor.x != g_previous_cursor_position.x ||
             cursor.y != g_previous_cursor_position.y)) {
            g_input_method.store(
                UiInputMethod::keyboard_mouse, std::memory_order_release);
        }
        g_previous_cursor_position = cursor;
        g_cursor_position_known = true;
    }
}

void LoadXInput() noexcept {
    if (g_xinput_module != nullptr || g_xinput_get_state != nullptr) {
        return;
    }
    constexpr std::array<const wchar_t*, 2u> names{{
        L"xinput1_4.dll", L"xinput9_1_0.dll"}};
    for (const wchar_t* name : names) {
        HMODULE module = LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (module == nullptr) {
            continue;
        }
        auto function = reinterpret_cast<XInputGetStateFunction>(
            GetProcAddress(module, "XInputGetState"));
        if (function != nullptr) {
            g_xinput_module = module;
            g_xinput_get_state = function;
            return;
        }
        FreeLibrary(module);
    }
}

void PollController() noexcept {
    LoadXInput();
    if (g_xinput_get_state == nullptr) {
        g_controller_connected = false;
        g_cursor_position_known = false;
        return;
    }
    XINPUT_STATE state{};
    if (g_xinput_get_state(0u, &state) != ERROR_SUCCESS) {
        g_controller_connected = false;
        g_previous_controller_buttons = 0u;
        return;
    }
    g_controller_connected = true;
    const WORD pressed = static_cast<WORD>(
        state.Gamepad.wButtons & ~g_previous_controller_buttons);
    g_previous_controller_buttons = state.Gamepad.wButtons;
    struct ButtonAction final {
        WORD button;
        UiInputAction action;
    };
    constexpr std::array<ButtonAction, 8u> buttons{{
        {XINPUT_GAMEPAD_DPAD_UP, UiInputAction::previous_item},
        {XINPUT_GAMEPAD_DPAD_DOWN, UiInputAction::next_item},
        {XINPUT_GAMEPAD_A, UiInputAction::activate},
        {XINPUT_GAMEPAD_X, UiInputAction::pause_resume},
        {XINPUT_GAMEPAD_B, UiInputAction::close_page},
        {XINPUT_GAMEPAD_LEFT_SHOULDER, UiInputAction::seek_backward},
        {XINPUT_GAMEPAD_RIGHT_SHOULDER, UiInputAction::seek_forward},
        {XINPUT_GAMEPAD_Y, UiInputAction::toggle_presentation},
    }};
    for (const ButtonAction& button : buttons) {
        if ((pressed & button.button) != 0u) {
            QueueAction(button.action, UiInputMethod::controller);
        }
    }
}

TileValue* FindValue(Tile* tile, const std::uint32_t id) noexcept {
    if (tile == nullptr || tile->values == nullptr || tile->value_count > kMaxTileValues) {
        return nullptr;
    }
    std::uint32_t low = 0;
    std::uint32_t high = tile->value_count;
    while (low < high) {
        const std::uint32_t middle = low + ((high - low) / 2u);
        TileValue* value = tile->values[middle];
        if (value == nullptr) {
            return nullptr;
        }
        if (value->id == id) {
            return value;
        }
        if (value->id < id) {
            low = middle + 1u;
        } else {
            high = middle;
        }
    }
    return nullptr;
}

bool TileHasName(const Tile* tile, const char* expected) noexcept {
    if (tile == nullptr || tile->name.data == nullptr || expected == nullptr || tile->name.length > 255u) {
        return false;
    }
    const std::size_t expected_length = std::strlen(expected);
    return expected_length == tile->name.length &&
           std::memcmp(tile->name.data, expected, expected_length) == 0;
}

Tile* FindDescendant(Tile* root, const char* name) noexcept {
    if (root == nullptr || name == nullptr) {
        return nullptr;
    }
    std::array<Tile*, kMaxTilesVisited> pending{};
    std::size_t pending_count = 0;
    pending[pending_count++] = root;
    std::size_t visited = 0;

    while (pending_count > 0 && visited++ < kMaxTilesVisited) {
        Tile* current = pending[--pending_count];
        if (TileHasName(current, name)) {
            return current;
        }

        ListNode* list_node = &current->children;
        std::size_t sibling_guard = 0;
        while (list_node != nullptr && sibling_guard++ < kMaxTilesVisited) {
            auto* child_node = static_cast<ChildNode*>(list_node->data);
            if (child_node != nullptr && child_node->child != nullptr && pending_count < pending.size()) {
                pending[pending_count++] = child_node->child;
            }
            list_node = list_node->next;
        }
    }
    return nullptr;
}

struct KeyLabel final {
    std::array<char, 16u> text{};
};

KeyLabel KeyLabelForScanCode(const std::uint32_t scan_code) noexcept {
    KeyLabel label{};
    LONG key_name_parameter = static_cast<LONG>((scan_code & 0x7Fu) << 16u);
    if ((scan_code & 0x80u) != 0u) {
        key_name_parameter |= 1l << 24u;
    }
    const int characters = GetKeyNameTextA(
        key_name_parameter, label.text.data(),
        static_cast<int>(label.text.size()));
    if (characters <= 0) {
        _snprintf_s(
            label.text.data(), label.text.size(), _TRUNCATE,
            "KEY %u", scan_code);
    }
    for (char& character : label.text) {
        if (character >= 'a' && character <= 'z') {
            character = static_cast<char>(character - ('a' - 'A'));
        }
    }
    return label;
}

const char* CatalogPromptText(const UiInputMethod input_method) noexcept {
    if (input_method == UiInputMethod::controller) {
        return "A PLAY  D-PAD SELECT";
    }
    const KeyLabel activate = KeyLabelForScanCode(g_input_settings.select_or_play);
    const KeyLabel previous = KeyLabelForScanCode(g_input_settings.previous_item);
    const KeyLabel next = KeyLabelForScanCode(g_input_settings.next_item);
    _snprintf_s(
        g_catalog_prompt.data(), g_catalog_prompt.size(), _TRUNCATE,
        "%s PLAY  %s/%s SELECT",
        activate.text.data(), previous.text.data(), next.text.data());
    return g_catalog_prompt.data();
}

const char* CatalogBackPromptText(const UiInputMethod input_method) noexcept {
    if (input_method == UiInputMethod::controller) {
        return "B BACK";
    }
    const KeyLabel back = KeyLabelForScanCode(g_input_settings.back_or_stop);
    _snprintf_s(
        g_catalog_back_prompt.data(), g_catalog_back_prompt.size(), _TRUNCATE,
        "%s BACK", back.text.data());
    return g_catalog_back_prompt.data();
}

const char* PlaybackStatusText(
    const PlaybackStateSnapshot& playback,
    const UiInputMethod input_method) noexcept {
    const bool controller = input_method == UiInputMethod::controller;
    const KeyLabel pause = KeyLabelForScanCode(g_input_settings.pause_resume);
    const KeyLabel back = KeyLabelForScanCode(g_input_settings.back_or_stop);
    const KeyLabel seek_backward = KeyLabelForScanCode(g_input_settings.seek_backward);
    const KeyLabel seek_forward = KeyLabelForScanCode(g_input_settings.seek_forward);
    const KeyLabel toggle = KeyLabelForScanCode(g_input_settings.toggle_color);
    switch (playback.state) {
        case PlaybackState::unavailable: return "PLAYER UNAVAILABLE";
        case PlaybackState::idle: return "VIDEOS";
        case PlaybackState::opening: return "OPENING VIDEO";
        case PlaybackState::buffering:
            if (playback.pause_after_buffering) {
                _snprintf_s(
                    g_playback_prompt.data(), g_playback_prompt.size(), _TRUNCATE,
                    "BUFFERING PAUSED  %s RESUME  %s STOP",
                    controller ? "X" : pause.text.data(),
                    controller ? "B" : back.text.data());
                return g_playback_prompt.data();
            }
            _snprintf_s(
                g_playback_prompt.data(), g_playback_prompt.size(), _TRUNCATE,
                "BUFFERING  %s STOP", controller ? "B" : back.text.data());
            return g_playback_prompt.data();
        case PlaybackState::playing:
            _snprintf_s(
                g_playback_prompt.data(), g_playback_prompt.size(), _TRUNCATE,
                "PLAYING  %s PAUSE  %s STOP  %s/%s SEEK  %s COLOR",
                controller ? "X" : pause.text.data(),
                controller ? "B" : back.text.data(),
                controller ? "LB" : seek_backward.text.data(),
                controller ? "RB" : seek_forward.text.data(),
                controller ? "Y" : toggle.text.data());
            return g_playback_prompt.data();
        case PlaybackState::paused:
            _snprintf_s(
                g_playback_prompt.data(), g_playback_prompt.size(), _TRUNCATE,
                "PAUSED  %s RESUME  %s STOP  %s/%s SEEK  %s COLOR",
                controller ? "X" : pause.text.data(),
                controller ? "B" : back.text.data(),
                controller ? "LB" : seek_backward.text.data(),
                controller ? "RB" : seek_forward.text.data(),
                controller ? "Y" : toggle.text.data());
            return g_playback_prompt.data();
        case PlaybackState::stopping: return "STOPPING";
        case PlaybackState::error:
            switch (playback.error) {
                case PlaybackError::media_open_failed: return "VIDEO COULD NOT BE OPENED";
                case PlaybackError::decoder_failed: return "VIDEO DECODE ERROR";
                case PlaybackError::decoder_memory_failed: return "VIDEO MEMORY ERROR";
                case PlaybackError::audio_initialization_failed:
                case PlaybackError::audio_device_failed:
                case PlaybackError::audio_stream_failed:
                    return "AUDIO PLAYBACK ERROR";
                case PlaybackError::clock_unavailable: return "PLAYBACK CLOCK ERROR";
                case PlaybackError::render_failed: return "VIDEO DISPLAY ERROR";
                case PlaybackError::invalid_state:
                case PlaybackError::none:
                    return "PLAYBACK ERROR";
            }
    }
    return "PLAYBACK ERROR";
}

const char* ResolveStatusName(const ResolveStatus status) noexcept {
    switch (status) {
        case ResolveStatus::kMenuArrayUnavailable:
            return "menu array unavailable";
        case ResolveStatus::kMenuRootUnavailable:
            return "MapMenu root unavailable";
        case ResolveStatus::kVideoRectUnavailable:
            return "PBVP_VideoRect unavailable";
        case ResolveStatus::kVideoSizeUnavailable:
            return "video width or height trait unavailable";
        case ResolveStatus::kCanvasExtentUnavailable:
            return "logical UI canvas width or height unavailable";
        case ResolveStatus::kParentChainInvalid:
            return "video rectangle parent chain invalid or hidden";
        case ResolveStatus::kGeometryInvalid:
            return "video rectangle geometry invalid";
        case ResolveStatus::kAccessViolation:
            return "guarded UI memory access failed";
        case ResolveStatus::kMapHidden:
            return "MapMenu hidden";
        case ResolveStatus::kResolved:
            return "resolved";
    }
    return "unknown UI resolution failure";
}

ResolveStatus ReadResolvedRect(UiRectSnapshot& output) noexcept {
    __try {
        const auto* visible = reinterpret_cast<const std::uint8_t*>(kMenuVisibilityArray);
        if (visible[kMapMenuType] == 0u) {
            return ResolveStatus::kMapHidden;
        }

        auto*** menu_array_pointer = reinterpret_cast<Tile***>(kTileMenuArrayPointer);
        if (menu_array_pointer == nullptr || *menu_array_pointer == nullptr) {
            return ResolveStatus::kMenuArrayUnavailable;
        }
        Tile* menu_root = (*menu_array_pointer)[kMapMenuType - kMenuTypeMin];
        if (menu_root == nullptr) {
            return ResolveStatus::kMenuRootUnavailable;
        }
        Tile* video_rect = FindDescendant(menu_root, "PBVP_VideoRect");
        if (video_rect == nullptr) {
            return ResolveStatus::kVideoRectUnavailable;
        }

        TileValue* width = FindValue(video_rect, kValueWidth);
        TileValue* height = FindValue(video_rect, kValueHeight);
        if (width == nullptr || height == nullptr) {
            return ResolveStatus::kVideoSizeUnavailable;
        }
        TileValue* canvas_width = nullptr;
        TileValue* canvas_height = nullptr;
        Tile* canvas = menu_root;
        std::size_t canvas_depth = 0;
        while (canvas != nullptr && canvas_depth++ < kMaxParentDepth) {
            TileValue* candidate_width = FindValue(canvas, kValueWidth);
            TileValue* candidate_height = FindValue(canvas, kValueHeight);
            if (candidate_width != nullptr && candidate_height != nullptr &&
                std::isfinite(candidate_width->number) && std::isfinite(candidate_height->number) &&
                candidate_width->number > 0.0f && candidate_height->number > 0.0f) {
                canvas_width = candidate_width;
                canvas_height = candidate_height;
                break;
            }
            canvas = canvas->parent;
        }
        if (canvas_width == nullptr || canvas_height == nullptr) {
            return ResolveStatus::kCanvasExtentUnavailable;
        }

        float x = 0.0f;
        float y = 0.0f;
        bool visible_chain = true;
        Tile* current = video_rect;
        std::size_t depth = 0;
        while (current != nullptr && depth++ < kMaxParentDepth) {
            if (TileValue* value_x = FindValue(current, kValueX); value_x != nullptr) {
                x += value_x->number;
            }
            if (TileValue* value_y = FindValue(current, kValueY); value_y != nullptr) {
                y += value_y->number;
            }
            if (TileValue* value_visible = FindValue(current, kValueVisible);
                value_visible != nullptr && value_visible->number <= 0.0f) {
                visible_chain = false;
            }
            if (current == menu_root) {
                break;
            }
            current = current->parent;
        }
        if (current != menu_root) {
            return ResolveStatus::kParentChainInvalid;
        }

        output.rect = {x, y, x + width->number, y + height->number};
        output.ui_extent = {canvas_width->number, canvas_height->number};
        const bool valid_geometry = std::isfinite(x) && std::isfinite(y) &&
                                    width->number > 0.0f && height->number > 0.0f &&
                                    canvas_width->number > 0.0f &&
                                    canvas_height->number > 0.0f;
        output.visible = valid_geometry && visible_chain;
        return valid_geometry ? ResolveStatus::kResolved : ResolveStatus::kGeometryInvalid;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return ResolveStatus::kAccessViolation;
    }
}

} // namespace

const char* UiSurfaceStatusName(const UiSurfaceStatus status) noexcept {
    switch (status) {
        case UiSurfaceStatus::available:
            return "available";
        case UiSurfaceStatus::wrong_thread:
            return "game and render callbacks use different threads";
        case UiSurfaceStatus::map_hidden:
            return "MapMenu hidden";
        case UiSurfaceStatus::menu_unavailable:
            return "MapMenu root unavailable";
        case UiSurfaceStatus::image_unavailable:
            return "PBVP_VideoSurface unavailable";
        case UiSurfaceStatus::wrong_tile_type:
            return "PBVP_VideoSurface is not a reviewed TileImage";
        case UiSurfaceStatus::source_texture_unavailable:
            return "TileImage source texture unavailable";
        case UiSurfaceStatus::wrong_shader_property_type:
            return "TileImage shader property has an unknown type";
        case UiSurfaceStatus::wrong_source_texture_type:
            return "TileImage source texture is not a reviewed NiSourceTexture";
        case UiSurfaceStatus::renderer_data_unavailable:
            return "NiTexture renderer data unavailable";
        case UiSurfaceStatus::wrong_renderer_data_type:
            return "texture renderer data has an unknown type";
        case UiSurfaceStatus::d3d_texture_unavailable:
            return "Direct3D base texture unavailable";
        case UiSurfaceStatus::access_violation:
            return "guarded UI texture access failed";
    }
    return "unknown UI surface failure";
}

UiBridge& UiBridge::Instance() noexcept {
    static UiBridge bridge;
    return bridge;
}

void UiBridge::UpdateOnGameThread() noexcept {
    if (!polling_logged_) {
        PBVP_LOG_INFO("xNVSE game-thread UI polling active");
        polling_logged_ = true;
    }
    UiRectSnapshot snapshot{};
    snapshot.game_thread_id = GetCurrentThreadId();
    const ResolveStatus status = ReadResolvedRect(snapshot);
    if (status == ResolveStatus::kResolved) {
        snapshot.generation = generation_.load(std::memory_order_relaxed) + 1u;
        if (!found_logged_) {
            PBVP_LOG_INFO(
                "UIO video rectangle resolved: left=%.2f top=%.2f right=%.2f bottom=%.2f canvas=%.2fx%.2f",
                snapshot.rect.left, snapshot.rect.top, snapshot.rect.right, snapshot.rect.bottom,
                snapshot.ui_extent.width, snapshot.ui_extent.height);
            found_logged_ = true;
        }
    } else {
        snapshot.visible = false;
        if (status != ResolveStatus::kMapHidden) {
            if (!map_visible_logged_) {
                PBVP_LOG_INFO("MapMenu became visible; resolving the UIO video rectangle");
                map_visible_logged_ = true;
            }
            const auto failure = static_cast<std::uint32_t>(status);
            if (last_failure_ != failure) {
                PBVP_LOG_WARN("Pip-Boy UI rectangle unavailable: %s", ResolveStatusName(status));
                last_failure_ = failure;
            }
        }
    }
    Publish(snapshot);
}

void UiBridge::SetGameInputState(const std::uintptr_t input_state) noexcept {
    g_nvse_input_state.store(input_state, std::memory_order_release);
    g_key_down.fill(false);
}

bool UiBridge::SetInputBindings(const InputSettings& settings) noexcept {
    if (!InputSettingsValid(settings)) {
        return false;
    }
    g_input_settings = settings;
    g_key_down.fill(false);
    last_status_state_ = PlaybackState::unavailable;
    return true;
}

bool SetTileFloat(Tile* tile, const std::uint32_t id, const float number) noexcept {
    if (tile == nullptr) {
        return false;
    }
    TileValue* value = FindValue(tile, id);
    if (value == nullptr || value->parent != tile) {
        return false;
    }
    if (value->number == number) {
        return true;
    }
    using SetFloatValue = void(__thiscall*)(void*, std::uint32_t, float, bool);
    const auto set_float = reinterpret_cast<SetFloatValue>(kTileSetFloatValueAddress);
    set_float(tile, id, number, true);
    return true;
}

bool SetTileString(Tile* tile, const char* text) noexcept {
    if (tile == nullptr || text == nullptr) {
        return false;
    }
    TileValue* value = FindValue(tile, kValueString);
    if (value == nullptr || value->parent != tile) {
        return false;
    }
    if (value->string != nullptr && std::strcmp(value->string, text) == 0) {
        return true;
    }
    using SetStringValue = void(__thiscall*)(void*, std::uint32_t, const char*, bool);
    const auto set_string = reinterpret_cast<SetStringValue>(kTileSetStringValueAddress);
    set_string(tile, kValueString, text, true);
    return true;
}

std::string WideToUiString(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    BOOL used_default = FALSE;
    const int required = WideCharToMultiByte(
        1252u, WC_NO_BEST_FIT_CHARS, text.data(), static_cast<int>(text.size()),
        nullptr, 0, "?", &used_default);
    if (required <= 0 || required > 1024) {
        return "?";
    }
    std::string converted(static_cast<std::size_t>(required), '\0');
    used_default = FALSE;
    if (WideCharToMultiByte(
            1252u, WC_NO_BEST_FIT_CHARS, text.data(), static_cast<int>(text.size()),
            converted.data(), required, "?", &used_default) != required) {
        return "?";
    }
    return converted;
}

bool SetCatalogStringsGuarded(
    const std::array<const char*, kUiCatalogVisibleRows>& rows,
    const char* prompt_text,
    const char* back_text) noexcept {
    __try {
        Tile* menu_root = CurrentMapMenuRoot();
        if (menu_root == nullptr) {
            return false;
        }
        bool accepted = true;
        for (std::size_t index = 0u; index < rows.size(); ++index) {
            char tile_name[32]{};
            const int name_length = _snprintf_s(
                tile_name, sizeof(tile_name), _TRUNCATE,
                "PBVP_RowText%zu", index);
            if (name_length <= 0) {
                return false;
            }
            Tile* row_text = FindDescendant(menu_root, tile_name);
            accepted = SetTileString(row_text, rows[index]) && accepted;
        }
        Tile* prompt = FindDescendant(menu_root, "PBVP_CatalogPrompt");
        Tile* back = FindDescendant(menu_root, "PBVP_BackText");
        return SetTileString(prompt, prompt_text) &&
               SetTileString(back, back_text) && accepted;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool SetNamedStringGuarded(const char* tile_name, const char* text) noexcept {
    __try {
        Tile* menu_root = CurrentMapMenuRoot();
        Tile* tile = FindDescendant(menu_root, tile_name);
        return SetTileString(tile, text);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

std::wstring ClipUiLabel(const std::wstring& text, const std::size_t limit) {
    if (text.size() <= limit) {
        return text;
    }
    if (limit <= 3u) {
        return text.substr(0u, limit);
    }
    std::wstring clipped = text.substr(0u, limit - 3u);
    clipped.append(L"...");
    return clipped;
}

std::string FormatPlaybackDetails(
    const std::wstring& title,
    const std::int64_t current_time_us,
    const std::int64_t duration_us) {
    const auto seconds = [](const std::int64_t microseconds) noexcept {
        return microseconds > 0 ? microseconds / 1'000'000ll : 0ll;
    };
    const std::int64_t current = seconds(current_time_us);
    const std::int64_t duration = seconds(duration_us);
    const std::wstring clipped = ClipUiLabel(title.empty() ? L"VIDEO" : title, 42u);
    std::string converted = WideToUiString(clipped);
    char time[48]{};
    _snprintf_s(
        time, sizeof(time), _TRUNCATE,
        "  %02lld:%02lld / %02lld:%02lld",
        current / 60ll, current % 60ll,
        duration / 60ll, duration % 60ll);
    converted.append(time);
    return converted;
}

void UiBridge::UpdateInputOnGameThread(const bool videos_page_active) noexcept {
    const std::uint32_t expected_thread = game_thread_id_.load(std::memory_order_acquire);
    if (expected_thread == 0u || GetCurrentThreadId() != expected_thread) {
        g_videos_page_active.store(false, std::memory_order_release);
        menu_input_available_ = false;
        map_menu_visible_ = false;
        return;
    }
    g_videos_page_active.store(videos_page_active, std::memory_order_release);
    Tile* menu_root = nullptr;
    __try {
        const auto* visible = reinterpret_cast<const std::uint8_t*>(kMenuVisibilityArray);
        if (visible[kMapMenuType] != 0u) {
            menu_root = CurrentMapMenuRoot();
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        menu_root = nullptr;
    }
    menu_input_available_ = AttachMapMenuInput(menu_root);
    map_menu_visible_ = menu_root != nullptr;
    if (menu_input_available_) {
        if (!g_input_hook_logged) {
            PBVP_LOG_INFO("Scoped MapMenu input bridge attached after vtable validation");
            g_input_hook_logged = true;
        }
    } else if (menu_root != nullptr && !g_input_hook_failure_logged) {
        PBVP_LOG_WARN(
            "Scoped MapMenu input bridge refused an unknown or occupied menu vtable");
        g_input_hook_failure_logged = true;
    }

    if (videos_page_active && menu_input_available_) {
        PollKeyboardAndMouse();
        PollController();
    } else {
        g_key_down.fill(false);
        g_previous_controller_buttons = 0u;
        g_controller_connected = false;
    }
}

UiInputSnapshot UiBridge::TakeInputSnapshot() noexcept {
    UiInputSnapshot snapshot{};
    snapshot.actions = g_pending_input_actions.exchange(0u, std::memory_order_acq_rel);
    snapshot.method = g_input_method.load(std::memory_order_acquire);
    snapshot.map_menu_visible = map_menu_visible_;
    snapshot.menu_hook_available = menu_input_available_;
    snapshot.controller_connected = g_controller_connected;
    return snapshot;
}

bool UiBridge::SetLayerEnabled(const bool enabled) noexcept {
    const std::uint32_t expected_thread = game_thread_id_.load(std::memory_order_acquire);
    if (expected_thread == 0u || GetCurrentThreadId() != expected_thread) {
        return false;
    }
    __try {
        auto*** menu_array_pointer = reinterpret_cast<Tile***>(kTileMenuArrayPointer);
        if (menu_array_pointer == nullptr || *menu_array_pointer == nullptr) {
            return false;
        }
        Tile* menu_root = (*menu_array_pointer)[kMapMenuType - kMenuTypeMin];
        if (menu_root == nullptr) {
            return false;
        }
        Tile* pbvp_root = FindDescendant(menu_root, "PBVP_Root");
        if (pbvp_root == nullptr) {
            return false;
        }
        TileValue* visible = FindValue(pbvp_root, kValueVisible);
        if (visible == nullptr || visible->parent != pbvp_root) {
            return false;
        }
        const std::uintptr_t root_address = reinterpret_cast<std::uintptr_t>(pbvp_root);
        if (last_root_tile_ == root_address && last_layer_enabled_ == enabled &&
            (visible->number > 0.0f) == enabled) {
            return true;
        }
        using SetFloatValue = void(__thiscall*)(void*, std::uint32_t, float, bool);
        const auto set_float = reinterpret_cast<SetFloatValue>(kTileSetFloatValueAddress);
        set_float(pbvp_root, kValueVisible, enabled ? 1.0f : 0.0f, true);
        last_root_tile_ = root_address;
        last_layer_enabled_ = enabled;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        last_root_tile_ = 0u;
        return false;
    }
}

bool UiBridge::SetPipBoyTintEnabled(const bool enabled) noexcept {
    const std::uint32_t expected_thread = game_thread_id_.load(std::memory_order_acquire);
    if (expected_thread == 0u || GetCurrentThreadId() != expected_thread) {
        return false;
    }
    __try {
        auto*** menu_array_pointer = reinterpret_cast<Tile***>(kTileMenuArrayPointer);
        if (menu_array_pointer == nullptr || *menu_array_pointer == nullptr) {
            return false;
        }
        Tile* menu_root = (*menu_array_pointer)[kMapMenuType - kMenuTypeMin];
        if (menu_root == nullptr) {
            return false;
        }
        Tile* surface = FindDescendant(menu_root, "PBVP_VideoSurface");
        if (surface == nullptr ||
            reinterpret_cast<std::uintptr_t>(surface->vtable) != kTileImageVtable) {
            return false;
        }
        TileValue* system_color = FindValue(surface, kValueSystemColor);
        if (system_color == nullptr || system_color->parent != surface) {
            return false;
        }
        const std::uintptr_t surface_address = reinterpret_cast<std::uintptr_t>(surface);
        if (last_surface_tile_ == surface_address &&
            last_pipboy_tint_enabled_ == enabled &&
            (system_color->number > 0.0f) == enabled) {
            return true;
        }
        using SetFloatValue = void(__thiscall*)(void*, std::uint32_t, float, bool);
        const auto set_float = reinterpret_cast<SetFloatValue>(kTileSetFloatValueAddress);
        set_float(surface, kValueSystemColor, enabled ? 1.0f : 0.0f, true);
        last_surface_tile_ = surface_address;
        last_pipboy_tint_enabled_ = enabled;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        last_surface_tile_ = 0u;
        return false;
    }
}

bool UiBridge::SetVideosMode(const UiVideosMode mode) noexcept {
    const std::uint32_t expected_thread = game_thread_id_.load(std::memory_order_acquire);
    if (expected_thread == 0u || GetCurrentThreadId() != expected_thread) {
        return false;
    }
    __try {
        Tile* menu_root = CurrentMapMenuRoot();
        if (menu_root == nullptr) {
            return false;
        }
        Tile* open_button = FindDescendant(menu_root, "PBVP_OpenButton");
        Tile* catalog_panel = FindDescendant(menu_root, "PBVP_CatalogPanel");
        Tile* video_rect = FindDescendant(menu_root, "PBVP_VideoRect");
        if (open_button == nullptr || catalog_panel == nullptr || video_rect == nullptr) {
            return false;
        }
        return SetTileFloat(
                   open_button, kValueVisible,
                   mode == UiVideosMode::data_page ? 1.0f : 0.0f) &&
               SetTileFloat(
                   catalog_panel, kValueVisible,
                   mode == UiVideosMode::catalog ? 1.0f : 0.0f) &&
               SetTileFloat(
                   video_rect, kValueVisible,
                   mode == UiVideosMode::playback ? 1.0f : 0.0f);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool UiBridge::SetCatalogRows(
    const std::array<std::wstring, kUiCatalogVisibleRows>& rows,
    const std::size_t row_count,
    const std::size_t selected_row,
    const UiInputMethod input_method) noexcept {
    const std::uint32_t expected_thread = game_thread_id_.load(std::memory_order_acquire);
    if (expected_thread == 0u || GetCurrentThreadId() != expected_thread ||
        row_count > rows.size() || (row_count > 0u && selected_row >= row_count)) {
        return false;
    }
    try {
        std::array<std::string, kUiCatalogVisibleRows> converted_rows{};
        std::array<const char*, kUiCatalogVisibleRows> row_pointers{};
        for (std::size_t index = 0u; index < rows.size(); ++index) {
            std::wstring displayed;
            if (index < row_count) {
                displayed.reserve(rows[index].size() + 2u);
                displayed.append(index == selected_row ? L"> " : L"  ");
                displayed.append(ClipUiLabel(rows[index], 48u));
            }
            converted_rows[index] = WideToUiString(displayed);
            row_pointers[index] = converted_rows[index].c_str();
        }
        return SetCatalogStringsGuarded(
            row_pointers,
            CatalogPromptText(input_method),
            CatalogBackPromptText(input_method));
    } catch (...) {
        return false;
    }
}

bool UiBridge::SetPlaybackStatus(
    const PlaybackStateSnapshot& playback,
    const UiInputMethod input_method) noexcept {
    const std::uint32_t expected_thread = game_thread_id_.load(std::memory_order_acquire);
    if (expected_thread == 0u || GetCurrentThreadId() != expected_thread) {
        return false;
    }
    __try {
        const auto* visible = reinterpret_cast<const std::uint8_t*>(kMenuVisibilityArray);
        if (visible[kMapMenuType] == 0u) {
            return false;
        }
        auto*** menu_array_pointer = reinterpret_cast<Tile***>(kTileMenuArrayPointer);
        if (menu_array_pointer == nullptr || *menu_array_pointer == nullptr) {
            return false;
        }
        Tile* menu_root = (*menu_array_pointer)[kMapMenuType - kMenuTypeMin];
        Tile* status_tile = FindDescendant(menu_root, "PBVP_LayerProbe");
        if (status_tile == nullptr) {
            return false;
        }
        TileValue* string_value = FindValue(status_tile, kValueString);
        if (string_value == nullptr || string_value->parent != status_tile) {
            return false;
        }
        const std::uintptr_t status_address = reinterpret_cast<std::uintptr_t>(status_tile);
        if (last_status_tile_ == status_address &&
            last_status_state_ == playback.state &&
            last_status_error_ == playback.error &&
            last_status_input_method_ == input_method) {
            return true;
        }

        using SetStringValue = void(__thiscall*)(
            void*, std::uint32_t, const char*, bool);
        const auto set_string = reinterpret_cast<SetStringValue>(
            kTileSetStringValueAddress);
        set_string(
            status_tile, kValueString,
            PlaybackStatusText(playback, input_method), true);
        last_status_tile_ = status_address;
        last_status_state_ = playback.state;
        last_status_error_ = playback.error;
        last_status_input_method_ = input_method;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        last_status_tile_ = 0u;
        return false;
    }
}

bool UiBridge::SetPlaybackDetails(
    const std::wstring& title,
    const std::int64_t current_time_us,
    const std::int64_t duration_us) noexcept {
    const std::uint32_t expected_thread = game_thread_id_.load(std::memory_order_acquire);
    if (expected_thread == 0u || GetCurrentThreadId() != expected_thread ||
        current_time_us < 0 || duration_us < 0) {
        return false;
    }
    try {
        const std::string details =
            FormatPlaybackDetails(title, current_time_us, duration_us);
        return SetNamedStringGuarded("PBVP_PlaybackDetails", details.c_str());
    } catch (...) {
        return false;
    }
}

UiSurfaceSnapshot UiBridge::ResolveSurfaceOnSharedThread(
    const std::uint32_t game_thread_id) const noexcept {
    UiSurfaceSnapshot output{};
    if (game_thread_id == 0u || GetCurrentThreadId() != game_thread_id) {
        output.status = UiSurfaceStatus::wrong_thread;
        return output;
    }

    __try {
        const auto* visible = reinterpret_cast<const std::uint8_t*>(kMenuVisibilityArray);
        if (visible[kMapMenuType] == 0u) {
            output.status = UiSurfaceStatus::map_hidden;
            return output;
        }
        auto*** menu_array_pointer = reinterpret_cast<Tile***>(kTileMenuArrayPointer);
        if (menu_array_pointer == nullptr || *menu_array_pointer == nullptr) {
            output.status = UiSurfaceStatus::menu_unavailable;
            return output;
        }
        Tile* menu_root = (*menu_array_pointer)[kMapMenuType - kMenuTypeMin];
        if (menu_root == nullptr) {
            output.status = UiSurfaceStatus::menu_unavailable;
            return output;
        }
        Tile* surface = FindDescendant(menu_root, "PBVP_VideoSurface");
        if (surface == nullptr) {
            output.status = UiSurfaceStatus::image_unavailable;
            return output;
        }
        if (reinterpret_cast<std::uintptr_t>(surface->vtable) != kTileImageVtable) {
            output.status = UiSurfaceStatus::wrong_tile_type;
            return output;
        }
        auto* image = reinterpret_cast<TileImage*>(surface);
        output.direct_texture = reinterpret_cast<std::uintptr_t>(image->direct_texture);
        output.shader_property = reinterpret_cast<std::uintptr_t>(image->shader_property);
        if (image->direct_texture != nullptr) {
            output.direct_texture_vtable =
                *reinterpret_cast<const std::uintptr_t*>(image->direct_texture);
        }

        NiTextureLayout* texture = nullptr;
        if (image->shader_property != nullptr) {
            output.shader_property_vtable =
                *reinterpret_cast<const std::uintptr_t*>(image->shader_property);
            if (output.shader_property_vtable != kTileShaderPropertyVtable) {
                output.status = UiSurfaceStatus::wrong_shader_property_type;
                return output;
            }
            auto* shader_property =
                static_cast<TileShaderPropertyLayout*>(image->shader_property);
            output.shader_source_texture =
                reinterpret_cast<std::uintptr_t>(shader_property->source_texture);
            if (shader_property->source_texture != nullptr) {
                output.shader_source_texture_vtable =
                    *reinterpret_cast<const std::uintptr_t*>(shader_property->source_texture);
                if (output.shader_source_texture_vtable != kNiSourceTextureVtable) {
                    output.status = UiSurfaceStatus::wrong_source_texture_type;
                    return output;
                }
                texture = static_cast<NiTextureLayout*>(shader_property->source_texture);
            }
        }

        if (texture == nullptr && image->direct_texture != nullptr) {
            if (output.direct_texture_vtable != kNiSourceTextureVtable) {
                output.status = UiSurfaceStatus::wrong_source_texture_type;
                return output;
            }
            texture = static_cast<NiTextureLayout*>(image->direct_texture);
        }
        if (texture == nullptr) {
            output.status = UiSurfaceStatus::source_texture_unavailable;
            return output;
        }
        auto* renderer_data = static_cast<NiDx9TextureDataLayout*>(texture->renderer_data);
        if (renderer_data == nullptr) {
            output.status = UiSurfaceStatus::renderer_data_unavailable;
            return output;
        }
        if (*reinterpret_cast<const std::uintptr_t*>(renderer_data) !=
            kNiDx9SourceTextureDataVtable) {
            output.status = UiSurfaceStatus::wrong_renderer_data_type;
            return output;
        }
        if (renderer_data->d3d_base_texture == nullptr) {
            output.status = UiSurfaceStatus::d3d_texture_unavailable;
            return output;
        }
        output.d3d_texture = reinterpret_cast<std::uintptr_t>(renderer_data->d3d_base_texture);
        output.status = UiSurfaceStatus::available;
        return output;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        output.status = UiSurfaceStatus::access_violation;
        return output;
    }
}

UiRectSnapshot UiBridge::ReadForRenderThread() const noexcept {
    UiRectSnapshot snapshot{};
    for (int attempt = 0; attempt < 4; ++attempt) {
        const std::uint32_t before = sequence_.load(std::memory_order_acquire);
        if ((before & 1u) != 0u) {
            continue;
        }
        snapshot.rect.left = left_.load(std::memory_order_relaxed);
        snapshot.rect.top = top_.load(std::memory_order_relaxed);
        snapshot.rect.right = right_.load(std::memory_order_relaxed);
        snapshot.rect.bottom = bottom_.load(std::memory_order_relaxed);
        snapshot.ui_extent.width = ui_width_.load(std::memory_order_relaxed);
        snapshot.ui_extent.height = ui_height_.load(std::memory_order_relaxed);
        snapshot.visible = visible_.load(std::memory_order_relaxed);
        snapshot.generation = generation_.load(std::memory_order_relaxed);
        snapshot.game_thread_id = game_thread_id_.load(std::memory_order_relaxed);
        const std::uint32_t after = sequence_.load(std::memory_order_acquire);
        if (before == after) {
            return snapshot;
        }
    }
    snapshot.visible = false;
    return snapshot;
}

void UiBridge::Clear() noexcept {
    UiRectSnapshot empty{};
    Publish(empty);
    found_logged_ = false;
    map_visible_logged_ = false;
    last_failure_ = 0u;
    last_status_tile_ = 0u;
    last_root_tile_ = 0u;
    last_surface_tile_ = 0u;
    last_layer_enabled_ = true;
    last_pipboy_tint_enabled_ = true;
    menu_input_available_ = false;
    map_menu_visible_ = false;
    g_videos_page_active.store(false, std::memory_order_release);
    g_pending_input_actions.store(0u, std::memory_order_release);
    g_previous_controller_buttons = 0u;
    g_controller_connected = false;
    g_cursor_position_known = false;
    g_key_down.fill(false);
    last_status_state_ = PlaybackState::unavailable;
    last_status_error_ = PlaybackError::none;
    last_status_input_method_ = UiInputMethod::keyboard_mouse;
}

void UiBridge::Publish(const UiRectSnapshot& snapshot) noexcept {
    sequence_.fetch_add(1u, std::memory_order_acq_rel);
    left_.store(snapshot.rect.left, std::memory_order_relaxed);
    top_.store(snapshot.rect.top, std::memory_order_relaxed);
    right_.store(snapshot.rect.right, std::memory_order_relaxed);
    bottom_.store(snapshot.rect.bottom, std::memory_order_relaxed);
    ui_width_.store(snapshot.ui_extent.width, std::memory_order_relaxed);
    ui_height_.store(snapshot.ui_extent.height, std::memory_order_relaxed);
    visible_.store(snapshot.visible, std::memory_order_relaxed);
    generation_.store(snapshot.generation, std::memory_order_relaxed);
    game_thread_id_.store(snapshot.game_thread_id, std::memory_order_relaxed);
    sequence_.fetch_add(1u, std::memory_order_release);
}

} // namespace pbvp
