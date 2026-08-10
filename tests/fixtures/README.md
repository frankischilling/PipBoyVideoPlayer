# Synthetic media fixtures

These MP4 files contain FFmpeg color bars and generated sine tones. They contain no personal recordings, copyrighted video clips, Bethesda assets, or material copied from another mod.

The files were generated twice with FFmpeg 8.0.1 from the Gyan full build. Both runs produced the same hashes. The exact generator executable has SHA-256 `74DB6C184A03DBA2BDFE23E1A1F41CF5A8385BC1DE6A7A1B26DB1DC541ABEF93`. The generator executable is not stored in this repository or included in a PBVP package.

- `h264-aac-44100-stereo.mp4` has 160x90 H.264 video and stereo AAC audio at 44.1 kHz.
- `h264-aac-rotate90.mp4` is the same stream data with a 90-degree display matrix.
- `h264-vfr-silent.mp4` is silent H.264 video with 100 ms and 200 ms presentation intervals.
- `unsupported-mpeg4-mp3.mp4` uses codecs excluded from the private PBVP runtime.

Run `scripts/generate-media-fixtures.ps1` to regenerate the files. The script refuses a different generator binary and verifies every output hash.
