# Synthetic media fixtures

These MP4 files contain FFmpeg color bars and generated sine tones. They contain no personal recordings, copyrighted video clips, Bethesda assets, or material copied from another mod.

Nine deterministic files were generated twice with FFmpeg 8.0.1 from the Gyan full build. Both runs produced the same hashes. The exact generator executable has SHA-256 `74DB6C184A03DBA2BDFE23E1A1F41CF5A8385BC1DE6A7A1B26DB1DC541ABEF93`. The generator executable is not stored in this repository or included in a PBVP package.

FFmpeg's CENC muxer writes fresh sample initialization vectors, so the encrypted output is not byte reproducible. The repository keeps one canonical `encrypted-cenc.mp4` fixture with SHA-256 `8B4436B8DF7717BFA52D55D929FA0738500D6D60D25BA7CE103E9C91890EF615`. The regeneration script verifies that file but does not replace it.

- `encrypted-cenc.mp4` carries the base streams as CENC AES-CTR encrypted samples.
- `h264-aac-1080p.mp4` has one second of 1920x1080 H.264 video and stereo 48 kHz AAC audio.
- `h264-aac-44100-stereo.mp4` has 160x90 H.264 video and stereo AAC audio at 44.1 kHz.
- `h264-aac-48000-51.mp4` has 5.1 AAC audio at 48 kHz for downmix tests.
- `h264-aac-48000-mono.mp4` has mono AAC audio at 48 kHz.
- `h264-aac-rotate90.mp4` is the same stream data with a 90-degree display matrix.
- `h264-title-metadata.mp4` has the file-level title `PBVP Metadata Title`.
- `h264-vfr-silent.mp4` is silent H.264 video with 100 ms and 200 ms presentation intervals.
- `h264-unsupported-mp3.mp4` pairs supported H.264 video with unsupported MP3 audio.
- `unsupported-mpeg4-mp3.mp4` uses codecs excluded from the private PBVP runtime.

Run `scripts/generate-media-fixtures.ps1` to regenerate the nine deterministic files. The script refuses a different generator binary and verifies every fixture hash.
