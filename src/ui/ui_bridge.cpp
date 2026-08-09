#include "pbvp/ui_bridge.hpp"

#include "pbvp/log.hpp"

#include "nvse/GameTiles.h"

#include <Windows.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace pbvp {
namespace {

constexpr std::uintptr_t kTileMenuArrayPointer = 0x011F350Cu;
constexpr std::uintptr_t kMenuVisibilityArray = 0x011F308Fu;
constexpr std::uintptr_t kTileImageVtable = 0x0106F01Cu;
constexpr std::uintptr_t kNiSourceTextureVtable = 0x0109B9ECu;
constexpr std::uintptr_t kNiDx9SourceTextureDataVtable = 0x010ED37Cu;
constexpr std::uint32_t kMenuTypeMin = 0x3E9u;
constexpr std::uint32_t kMapMenuType = 0x3FFu;
constexpr std::uint32_t kValueX = ::Tile::kTileValue_x;
constexpr std::uint32_t kValueY = ::Tile::kTileValue_y;
constexpr std::uint32_t kValueVisible = ::Tile::kTileValue_visible;
constexpr std::uint32_t kValueHeight = ::Tile::kTileValue_height;
constexpr std::uint32_t kValueWidth = ::Tile::kTileValue_width;
constexpr std::uint32_t kValueFilename = ::Tile::kTileValue_filename;
constexpr std::size_t kMaxTileValues = 4096;
constexpr std::size_t kMaxTilesVisited = 512;
constexpr std::size_t kMaxParentDepth = 64;
constexpr std::uint32_t kMaxSurfaceRefreshes = 8u;
constexpr char kSurfaceFilename[] = "Interface\\PipBoyVideoPlayer\\Surface.dds";

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
    void* texture;
    void* shader_property;
    std::uint8_t unknown_44[4];
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
static_assert(offsetof(TileImage, texture) == 0x3C);
static_assert(offsetof(NiTextureLayout, renderer_data) == 0x24);
static_assert(offsetof(NiDx9TextureDataLayout, d3d_base_texture) == 0x64);

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

TileImage* FindFirstTexturedImage(Tile* root, const Tile* excluded) noexcept {
    if (root == nullptr) {
        return nullptr;
    }
    std::array<Tile*, kMaxTilesVisited> pending{};
    std::size_t pending_count = 0;
    pending[pending_count++] = root;
    std::size_t visited = 0;

    while (pending_count > 0 && visited++ < kMaxTilesVisited) {
        Tile* current = pending[--pending_count];
        if (current != excluded &&
            reinterpret_cast<std::uintptr_t>(current->vtable) == kTileImageVtable) {
            auto* image = reinterpret_cast<TileImage*>(current);
            if (image->texture != nullptr) {
                return image;
            }
        }

        ListNode* list_node = &current->children;
        std::size_t sibling_guard = 0;
        while (list_node != nullptr && sibling_guard++ < kMaxTilesVisited) {
            auto* child_node = static_cast<ChildNode*>(list_node->data);
            if (child_node != nullptr && child_node->child != nullptr &&
                pending_count < pending.size()) {
                pending[pending_count++] = child_node->child;
            }
            list_node = list_node->next;
        }
    }
    return nullptr;
}

bool StringValueEquals(const TileValue* value, const char* expected) noexcept {
    if (value == nullptr || value->string == nullptr || expected == nullptr) {
        return false;
    }
    const std::size_t expected_length = std::strlen(expected);
    for (std::size_t index = 0; index <= expected_length; ++index) {
        if (value->string[index] != expected[index]) {
            return false;
        }
    }
    return true;
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
        if (current != menu_root || !visible_chain) {
            return ResolveStatus::kParentChainInvalid;
        }

        output.rect = {x, y, x + width->number, y + height->number};
        output.ui_extent = {canvas_width->number, canvas_height->number};
        output.visible = std::isfinite(x) && std::isfinite(y) &&
                         width->number > 0.0f && height->number > 0.0f &&
                         canvas_width->number > 0.0f && canvas_height->number > 0.0f;
        return output.visible ? ResolveStatus::kResolved : ResolveStatus::kGeometryInvalid;
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
        case UiSurfaceStatus::texture_unavailable:
            return "TileImage texture unavailable";
        case UiSurfaceStatus::wrong_texture_type:
            return "TileImage texture is not a reviewed NiSourceTexture";
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
        RefreshSurfaceTextureOnGameThread();
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

void UiBridge::RefreshSurfaceTextureOnGameThread() noexcept {
    __try {
        auto*** menu_array_pointer = reinterpret_cast<Tile***>(kTileMenuArrayPointer);
        if (menu_array_pointer == nullptr || *menu_array_pointer == nullptr) {
            return;
        }
        Tile* menu_root = (*menu_array_pointer)[kMapMenuType - kMenuTypeMin];
        if (menu_root == nullptr) {
            return;
        }
        Tile* surface = FindDescendant(menu_root, "PBVP_VideoSurface");
        if (surface == nullptr ||
            reinterpret_cast<std::uintptr_t>(surface->vtable) != kTileImageVtable) {
            return;
        }
        auto* image = reinterpret_cast<TileImage*>(surface);
        if (image->texture != nullptr ||
            last_refreshed_surface_ == reinterpret_cast<std::uintptr_t>(surface)) {
            return;
        }
        if (surface_refresh_count_ >= kMaxSurfaceRefreshes) {
            if (!surface_refresh_limit_logged_) {
                PBVP_LOG_WARN("Private UI surface filename refresh limit reached");
                surface_refresh_limit_logged_ = true;
            }
            return;
        }

        TileValue* filename = FindValue(surface, kValueFilename);
        if (filename == nullptr) {
            PBVP_LOG_WARN("PBVP_VideoSurface has no filename trait to refresh");
            last_refreshed_surface_ = reinterpret_cast<std::uintptr_t>(surface);
            return;
        }
        const bool filename_matches = StringValueEquals(filename, kSurfaceFilename);
        TileImage* reference = FindFirstTexturedImage(menu_root, surface);
        PBVP_LOG_INFO(
            "UI image field check before filename refresh: target[3C]=0x%08X target[40]=0x%08X reference[3C]=0x%08X reference[40]=0x%08X",
            static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(image->texture)),
            static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(image->shader_property)),
            static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(
                reference != nullptr ? reference->texture : nullptr)),
            static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(
                reference != nullptr ? reference->shader_property : nullptr)));

        last_refreshed_surface_ = reinterpret_cast<std::uintptr_t>(surface);
        ++surface_refresh_count_;
        auto* game_tile = reinterpret_cast<::Tile*>(surface);
        if (filename_matches) {
            CALL_MEMBER_FN(game_tile, SetStringValue)(kValueFilename, "", true);
        }
        CALL_MEMBER_FN(game_tile, SetStringValue)(kValueFilename, kSurfaceFilename, true);
        PBVP_LOG_INFO(
            "Private UI surface filename refreshed on the game thread; prior filename=%s request=%u",
            filename_matches ? "expected" : "different", surface_refresh_count_);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        PBVP_LOG_ERROR("Guarded private UI surface filename refresh failed");
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
        output.surface_texture_member = reinterpret_cast<std::uintptr_t>(image->texture);
        output.surface_shader_member = reinterpret_cast<std::uintptr_t>(image->shader_property);
        auto* texture = static_cast<NiTextureLayout*>(image->texture);
        if (texture == nullptr) {
            TileImage* reference = FindFirstTexturedImage(menu_root, surface);
            if (reference != nullptr) {
                output.reference_texture_member =
                    reinterpret_cast<std::uintptr_t>(reference->texture);
                output.reference_shader_member =
                    reinterpret_cast<std::uintptr_t>(reference->shader_property);
            }
            output.status = UiSurfaceStatus::texture_unavailable;
            return output;
        }
        if (*reinterpret_cast<const std::uintptr_t*>(texture) != kNiSourceTextureVtable) {
            output.status = UiSurfaceStatus::wrong_texture_type;
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
    last_refreshed_surface_ = 0u;
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
