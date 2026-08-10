# Third-party notices

The build uses headers from the official xNVSE 6.4.5 source release. xNVSE retains its own copyright and license. Its source is downloaded for local builds and is not included in the mod archive.

UIO's public registration convention is used to inject the Pip-Boy prefab. Pip-Boy Video Player does not distribute UIO or its assets.

Fallout: New Vegas names belong to their respective owners. The repository and mod archive contain no game executable, game asset, or media file.

The Phase 2 media runtime uses FFmpeg 8.1.2 from the official source archive at `https://ffmpeg.org/releases/ffmpeg-8.1.2.tar.xz`. The verified archive SHA-256 is `464BEB5E7BF0C311E68B45AE2F04E9CC2AF88851ABB4082231742A74D97B524C`. The detached signature was verified with FFmpeg release key `FCF986EA15E6E293A5644F10B4322F04D67658D8`.

The configured FFmpeg libraries report LGPL version 2.1 or later. The build does not enable GPL, version 3, or nonfree components. FFmpeg's `LICENSE.md` also requires Independent JPEG Group credit when executable files contain its derived IDCT sources. PBVP makes no changes to those FFmpeg sources. A binary package must include the matching upstream `LICENSE.md` and `COPYING.LGPLv2.1` files and the exact build manifest.

`avutil-60.dll` statically includes the required clock functions from mingw-w64 winpthreads package `13.0.0.r505.g7d006b2ea-1`, source commit `7d006b2ea4b17da66e515f4494b86cc1adb52f24`. That code is under MIT and BSD-3-Clause-Clear terms. A binary package must reproduce the package's `COPYING` notice.
