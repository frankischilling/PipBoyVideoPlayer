#pragma once

#include "pbvp/ffmpeg_runtime.hpp"

#include <string>

namespace pbvp {

class AudioSmokeTest final {
public:
    static AudioSmokeTest& Instance() noexcept;

    void Start(const FfmpegRuntime& runtime, std::wstring media_root) noexcept;
    void Update() noexcept;
    void Stop() noexcept;

private:
    AudioSmokeTest() = default;
    struct Impl;
    Impl* impl_{};
};

} // namespace pbvp
