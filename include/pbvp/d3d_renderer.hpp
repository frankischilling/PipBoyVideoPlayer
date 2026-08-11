#pragma once

#include "pbvp/configuration.hpp"
#include "pbvp/frame_cadence.hpp"
#include "pbvp/media_decoder.hpp"
#include "pbvp/rect_math.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>

struct IDirect3DDevice9;

namespace pbvp {

struct UiRectSnapshot;

struct D3dRendererSnapshot final {
    std::uint64_t frame_callbacks{};
    std::uint64_t visible_frames{};
    std::uint64_t submitted_video_frames{};
    std::uint64_t replaced_mailbox_frames{};
    std::uint64_t cleared_mailbox_frames{};
    std::uint64_t uploaded_video_frames{};
    std::uint64_t upload_attempts{};
    std::uint64_t upload_successes{};
    std::uint64_t upload_failures{};
    std::uint64_t last_video_generation{};
    std::int64_t last_video_pts_us{};
    double upload_minimum_us{};
    double upload_average_us{};
    double upload_maximum_us{};
    bool video_pixels_ready{};
    bool mailbox_occupied{};
};

class D3dRenderer final {
public:
    static D3dRenderer& Instance() noexcept;

    [[nodiscard]] bool SubmitVideoFrame(DecodedVideoFrame frame) noexcept;
    void ConfigurePresentation(AspectMode aspect_mode, TintMode tint_mode) noexcept;
    void ClearVideoFrame() noexcept;
    void OnFrame(const UiRectSnapshot& ui_rect) noexcept;
    void RequestShutdown() noexcept;
    [[nodiscard]] D3dRendererSnapshot Snapshot() const noexcept;

private:
    static constexpr std::uint32_t kTextureWidth = 256u;
    static constexpr std::uint32_t kTextureHeight = 256u;
    static constexpr std::size_t kPresentationBytes =
        static_cast<std::size_t>(kTextureWidth) * kTextureHeight * 4u;

    D3dRenderer() = default;

    IDirect3DDevice9* FindDevice() noexcept;
    bool ValidateDevice(IDirect3DDevice9* device) noexcept;
    std::optional<DecodedVideoFrame> TakePendingFrame() noexcept;
    bool PrepareVideoPixels(
        const DecodedVideoFrame& frame,
        PixelExtent presentation_extent) noexcept;
    void PrepareCheckerboard() noexcept;
    bool UploadPixels(IDirect3DDevice9* device, std::uintptr_t surface) noexcept;
    void RecordVisibleCadence() noexcept;
    void RecordUploadDuration(double microseconds) noexcept;
    void LogDeviceProfile(IDirect3DDevice9* device) noexcept;
    void LogSessionSummary() noexcept;
    void ReleaseResources() noexcept;

    mutable std::mutex mailbox_mutex_{};
    std::optional<DecodedVideoFrame> pending_frame_{};
    std::array<std::uint8_t, kPresentationBytes> presentation_pixels_{};
    std::atomic<bool> clear_requested_{false};
    std::atomic<std::uint32_t> aspect_mode_{
        static_cast<std::uint32_t>(AspectMode::fit)};
    std::atomic<std::uint32_t> tint_mode_{
        static_cast<std::uint32_t>(TintMode::pipboy)};
    IDirect3DDevice9* device_{};
    std::uintptr_t last_surface_{};
    std::uint64_t frame_callback_count_{};
    std::uint64_t visible_frame_count_{};
    std::uint64_t submitted_video_frame_count_{};
    std::uint64_t mailbox_replacement_count_{};
    std::uint64_t mailbox_clear_count_{};
    std::uint64_t uploaded_video_frame_count_{};
    std::uint64_t last_video_generation_{};
    std::int64_t last_video_pts_us_{};
    std::uint64_t upload_attempt_count_{};
    std::uint64_t upload_success_count_{};
    std::uint64_t upload_failure_count_{};
    std::uint64_t upload_timing_count_{};
    double upload_total_microseconds_{};
    double upload_minimum_microseconds_{};
    double upload_maximum_microseconds_{};
    FrameCadenceTracker cadence_tracker_{};
    std::int64_t cadence_frequency_{};
    std::uint32_t cadence_sample_count_{};
    double cadence_total_fps_{};
    double cadence_minimum_fps_{};
    double cadence_maximum_fps_{};
    std::uint32_t error_count_{};
    std::uint32_t device_validation_count_{};
    std::uint32_t last_surface_status_{};
    bool presentation_pixels_valid_{};
    bool video_pixels_ready_{};
    bool upload_retry_pending_{};
    bool shutdown_requested_{};
    bool frame_callback_logged_{};
    bool thread_identity_logged_{};
    bool video_upload_logged_{};
    bool summary_logged_{};
};

} // namespace pbvp
