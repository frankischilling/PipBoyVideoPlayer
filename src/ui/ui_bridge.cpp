#include "pbvp/ui_bridge.hpp"

#include "pbvp/log.hpp"

#include <Windows.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace pbvp {
namespace {

constexpr std::uintptr_t kTileMenuArrayPointer = 0x011F350Cu;
constexpr std::uintptr_t kMenuVisibilityArray = 0x011F308Fu;
constexpr std::uint32_t kMenuTypeMin = 0x3E9u;
constexpr std::uint32_t kMapMenuType = 0x3FFu;
constexpr std::uint32_t kValueX = 0x0FA1u;
constexpr std::uint32_t kValueY = 0x0FA2u;
constexpr std::uint32_t kValueVisible = 0x0FA3u;
constexpr std::uint32_t kValueHeight = 0x0FAFu;
constexpr std::uint32_t kValueWidth = 0x0FB0u;
constexpr std::size_t kMaxTileValues = 4096;
constexpr std::size_t kMaxTilesVisited = 512;
constexpr std::size_t kMaxParentDepth = 64;

enum class ResolveStatus : std::uint32_t {
    kMapHidden = 1u,
    kMenuArrayUnavailable,
    kMenuRootUnavailable,
    kVideoRectUnavailable,
    kVideoSizeUnavailable,
    kMenuExtentUnavailable,
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

static_assert(sizeof(Tile) == 0x38);
static_assert(offsetof(Tile, values) == 0x14);
static_assert(offsetof(Tile, name) == 0x20);
static_assert(offsetof(Tile, parent) == 0x28);

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
        case ResolveStatus::kMenuExtentUnavailable:
            return "MapMenu width or height trait unavailable";
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
        TileValue* root_width = FindValue(menu_root, kValueWidth);
        TileValue* root_height = FindValue(menu_root, kValueHeight);
        if (root_width == nullptr || root_height == nullptr) {
            return ResolveStatus::kMenuExtentUnavailable;
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
        output.ui_extent = {root_width->number, root_height->number};
        output.visible = std::isfinite(x) && std::isfinite(y) &&
                         width->number > 0.0f && height->number > 0.0f &&
                         root_width->number > 0.0f && root_height->number > 0.0f;
        return output.visible ? ResolveStatus::kResolved : ResolveStatus::kGeometryInvalid;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return ResolveStatus::kAccessViolation;
    }
}

} // namespace

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
    sequence_.fetch_add(1u, std::memory_order_release);
}

} // namespace pbvp
