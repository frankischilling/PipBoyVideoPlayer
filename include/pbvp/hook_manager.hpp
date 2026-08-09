#pragma once

namespace pbvp::hooks {

bool ProbeAndInstall() noexcept;
void MarkShutdown() noexcept;
bool IsReady() noexcept;

} // namespace pbvp::hooks
