# Third-party libraries

ZM links shared libraries directly and does not launch ffmpeg.exe for playback. The FFmpeg executable in the SDK is used only to generate test fixtures.

| Dependency | Build / version | License | Upstream |
| --- | --- | --- | --- |
| FFmpeg | n9.0.2-22-g46d8f462ee, 2026-10-05 LGPL shared build | LGPL 3 or later, plus bundled library notices | https://ffmpeg.org/ / https://github.com/BtbN/FFmpeg-Builds |
| SDL2 | 2.32.10 | zlib | https://github.com/libsdl-org/SDL |
| SDL2_ttf | 2.24.0 | zlib; bundled font engine notices also apply | https://github.com/libsdl-org/SDL_ttf |
| NanumSquareRound Regular/Bold | Official NAVER archive, unmodified | SIL OFL 1.1 | https://hangeul.naver.com/font |
| Noto Sans CJK SC Bold | Sans2.004, unmodified, source commit 523d033d6cb47f4a80c58a35753646f5c3608a78 | SIL OFL 1.1 | https://github.com/notofonts/noto-cjk |

FFmpeg includes libass for subtitle rendering and other codec libraries. SDK license notices are included in `third_party` and copied into the portable package. Dependency downloads are fixed by release URL and checked against SHA256 in `scripts/setup-deps.ps1`.

NanumSquareRound and Noto CJK are bundled in the `fonts` folder with their licenses and source records. Windows fallback fonts are loaded from the user's installation and are not redistributed. Visual C++ runtime DLLs are copied from the installed Visual Studio x64 CRT redistributable directory for app-local deployment.

The installer is built with Inno Setup 6.7.3 (https://jrsoftware.org/), using its license reproduced in `licenses/inno-setup.txt`. The compiler download is signed by Pyrsys B.V.; SHA256: `9C73C3BAE7ED48D44112A0F48E66742C00090BDB5BEF71D9D3C056C66E97B732`. The compiler is a build tool and is not included in ZM.
