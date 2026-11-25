# launcher — nx.rpg 统一前端（C++20 + Borealis）

`launcher/` 基于 **C++20 + [Borealis](https://github.com/xfangfang/borealis)**（Nintendo Switch 原生系统视觉风格的硬件加速 UI 框架）构建 `nx.rpg.nro`。

## 为什么选择 C++20 + Borealis？

1. **瞬时链式往返体验**：从 `cores/*.nro` 退出并通过 `envSetNextLoad` 返回前端时，原生 C++ + Borealis 冷启动耗时 `< 0.5s`，配合 `cache/library.json` 索引缓存可在 `< 50ms` 内恢复上次选中的游戏焦点。
2. **极低内存与显存驻留**：前端本身占用极小，且退出进入核心前完全释放 OpenGL/deko3d 上下文，把 100% 物理内存交给目标核心。
3. **双端开发支持**：Borealis 原生支持在 Windows / Linux / macOS 桌面端通过 GLFW/SDL2 编译运行，无需每次刷写 SD 卡即可在电脑上开发调试全部 UI 视图与交互。

## 目录结构

```text
launcher/
├── CMakeLists.txt                              # Switch NRO 与桌面预览双模式构建骨架
├── npdm.json                                   # Switch TitleID (0x016e787270670000) 与系统服务权限声明
├── include/launcher/
│   ├── app.hpp                                 # 应用生命周期、--from-core 恢复与 Applet 告警门控
│   ├── services/
│   │   ├── library_service.hpp                 # 增量扫描器、library.json 缓存读写与缩略图缓存生成
│   │   ├── core_dispatcher.hpp                 # 启动前自检、配置合并、launch.json 原子落盘与链式跳转
│   │   └── environment_doctor.hpp              # 核心 NRO、5 代共享 RTP、CJK 字体与文件系统健康度巡检
│   └── views/
│       ├── root_frame.hpp                      # 主 TabFrame（全部游戏 / 最近游玩 / 环境自检 / 全局设置）
│       ├── game_grid_tab.hpp                   # 封面墙与列表视图（支持按 7 代引擎筛选）
│       ├── game_detail_activity.hpp            # 游戏详情页与单游戏覆写设置（编码/CPU超频/缩放/RTP/Win32Stub）
│       └── diagnostics_tab.hpp                 # 最近运行崩溃日志查看器（读取 logs/games/<game_id>.log）
├── resources/
│   ├── i18n/                                   # 多语言资源包（zh-CN.json / en-US.json）
│   └── xml/                                    # Borealis 声明式 XML 视图模板目录
└── src/                                        # Phase 1 C++20 实现目录
```
