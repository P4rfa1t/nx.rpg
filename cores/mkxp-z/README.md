# cores/mkxp-z — mkxp-z Switch / devkitA64 移植层

- **上游源码路径**：`third_party/mkxp-z`
- **构建系统**：Meson（交叉编译至 `aarch64-none-elf` / `devkitA64`）
- **目标引擎**：RPG Maker XP（RGSS1） / RPG Maker VX（RGSS2） / RPG Maker VX Ace（RGSS3）
- **规划文档**：[docs/cores/mkxp-z-plan.md](../../docs/cores/mkxp-z-plan.md)

## 目录用途

- `cross/switch-aarch64.ini`：Meson 面向 Nintendo Switch（`devkitA64` + `libnx` + `portlibs/switch`）的交叉编译配置骨架。
- `patches/`：针对 `mkxp-z` 及静态 `libruby` 的 Switch 移植补丁队列（GLES2 适配、`launch.json` 接入、RGSS Win32API Stub 与 `envSetNextLoad` 返回）。
