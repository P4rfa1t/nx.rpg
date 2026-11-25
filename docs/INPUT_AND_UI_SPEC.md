# 输入映射、触控手势、软键盘与前端交互规范

本文档统一规范 Nintendo Switch 手柄、触屏与系统软键盘在 **Borealis 前端** 及 **7 代 RPG Maker 游戏核心** 中的交互映射标准。

---

## 1. 跨世代手柄标准按键矩阵（Joy-Con 双持 / Pro 手柄）

默认采用任天堂原生直觉布局（**`A` = 确认，`B` = 取消**）。若玩家在 `global.json` 或单游戏配置中开启 `"swapConfirmCancel": true`，则各核心统一在输入桥接层将 `A` 与 `B` 互换。

| Switch 物理输入 | Borealis 前端 (`nx.rpg.nro`) | EasyRPG (`rm2k` / `rm2k3`) | mkxp-z (`rmxp` / `rmvx` / `rmvxa`) | nxjs-mvz (`rmmv` / `rmmz`) |
| :--- | :--- | :--- | :--- | :--- |
| **十字键 / 左摇杆** | 焦点移动 | 角色移动 / 菜单光标 | `Input::UP / DOWN / LEFT / RIGHT` | `up / down / left / right` (`Arrow*`) |
| **`A` 键** | 进入 / 启动游戏 | 确认 (`Decision` / `Z` / `Enter`) | `Input::C` (`Enter` / `Space` / `Z`) | `ok` (`Enter` / `Z`) |
| **`B` 键** | 返回上一级 | 取消 / 菜单 (`Cancel` / `X` / `Esc`) | `Input::B` (`Esc` / `X` / `Insert`) | `escape` (`Escape` / `X` / `Insert`) |
| **`X` 键** | 刷新 / 收藏切换 | 菜单 / 辅助取消（起名时唤起软键盘） | `Input::Y` (`S` 键) | `menu` / 快捷菜单 (`X` / 唤起软键盘) |
| **`Y` 键** | 打开单游戏设置抽屉 | 按住加速行走 / `Shift` | `Input::A` (`Shift` 奔跑/加速) | `shift` (`Shift` 奔跑) |
| **`L` 键** | 上一页 Tab | 上翻页 (`PageUp` / `Q`) | `Input::L` (`Q` / `PageUp`) | `pageup` (`Q` / `PageUp`) |
| **`R` 键** | 下一页 Tab | 下翻页 / 帧率开关 (`PageDown` / `W`) | `Input::R` (`W` / `PageDown`) | `pagedown` (`W` / `PageDown`) |
| **`ZL` 键** | 按引擎向下筛选 | 调试行走穿透（仅测试模式）/ 辅助键 `Ctrl` | `Input::X` (`A` 键 / `Ctrl` 跳过文本) | `control` (`Control` 跳过已读对话) |
| **`ZR` 键** | 按引擎向上筛选 | 按住倍速快进 (`Fast-Forward`) | 按住倍速帧率切换（可选） | `tab` / 辅助功能键 |
| **`+` (Plus)** | 全局设置快捷入口 | EasyRPG 内置设置菜单 | 唤出键位提示 / `F1` 设置面板 | 暂停 / 快捷菜单辅助 |
| **`-` (Minus)** | 查看上次运行日志 | 游戏软重置 (`Reset`) | `F2` 显示/隐藏 FPS（长按 `F12` 软重置） | 切换 FPS 浮层显示 |
| **组合键 `L + R + +`** | — | **安全保存缓冲并退出回 `nx.rpg.nro`** | **安全保存缓冲并退出回 `nx.rpg.nro`** | **安全保存缓冲并退出回 `nx.rpg.nro`** |

### 单手柄横握支持（Single Joy-Con）

在仅连接单个横握 Joy-Con（`HidNpadStyleTag_NpadJoyLeft` / `JoyRight`）时，仅限无重度组合键需求的标准 JRPG：
- 摇杆映射方向、右侧按键映射确认、下方按键映射取消、`SL + SR + (+/-)` 映射返回前端。

---

## 2. 掌机模式触控交互规范（Handheld Touch）

当 `Switch.operationMode() == Handheld` 且 `runtime.enableTouch == true` 时：

1. **RPG Maker MV / MZ（原生最佳体验）**：
   - 单指点击屏幕：映射为画布等比换算后的 `touchstart` / `touchend`（点地寻路、点击菜单按钮）。
   - **双指同时轻触（Two-Finger Tap）**：遵循 MV/MZ 官方移动端习惯，直接触发 `TouchInput._onCancel()`（打开主菜单 / 取消返回）！
   - 画面黑边（Pillarbox 4:3 游戏在 16:9 屏幕两侧的留白区域）：自动忽略越界坐标，防止点到黑边误触地图边缘移动。
2. **EasyRPG Player（RM2000 / 2003）**：
   - 复用上游 `resources/switch/touch_ui.png` 掌机侧边触控栏，并在进入底座模式（Docked）时自动隐藏。
3. **mkxp-z（RMXP / VX / VXAce）**：
   - 将触屏单指坐标换算至逻辑分辨率（如 `640×480` 或 `544×416`）写入 `Input.mouse_x` / `Input.mouse_y`，使依赖 `Win32API` `GetCursorPos` + `LBUTTON` 的鼠标脚本游戏可直接在 Switch 掌机触屏操作！

---

## 3. Switch 系统软键盘（`swkbd`）集成方案

RPG Maker 游戏常见的“输入勇者名字”、“输入宝箱密码/兑换码”如果只靠游戏内置假名网格对中文玩家极不友好；而部分 MV/MZ 插件直接使用 HTML `<input type="text">` 在主机上更是无法打字。

当 `runtime.enableSoftwareKeyboard == true` 时：

- **nxjs-mvz**：
  - 拦截 DOM `<input>` / `<textarea>` 的 `.focus()` 调用，直接拉起 `nx.js` 内置的 Horizon OS 系统输入法（支持拼音、日文假名、英文与符号），确认后派发 `input` 与 `change` 事件。
  - 提供可选钩子增强 `Scene_Name`，允许按 `X` 键直接用系统中文/日文输入法改写角色姓名。
- **easyrpg & mkxp-z**：
  - 在补丁 `0003` / `0004` 中绑定 `libnx` `swkbdCreate(&kbd, 0)`，暂停音频推流与主循环渲染，弹出原生输入面板后回填 UTF-8（或转码为游戏目标编码）字符串。

---

## 4. Borealis 前端界面信息架构（`nx.rpg.nro`）

```text
+-----------------------------------------------------------------------------+
| nx.rpg  [Title Takeover: 3.2 GB OK]                    [排序: 最近] [筛选]  |
+------------------+----------------------------------------------------------+
| > 全部游戏 (24)  |  +------------+  +------------+  +------------+          |
|   最近游玩       |  |  [封面图]  |  |  [封面图]  |  |  [封面图]  |          |
|   环境与日志自检 |  |            |  |            |  |            |          |
|   全局设置       |  | 废都物语   |  | 魔女之家MV |  | To the Moon|          |
|                  |  | [RM2000]   |  | [RMMV]     |  | [RMXP]     |          |
|                  |  +------------+  +------------+  +------------+          |
+------------------+----------------------------------------------------------+
| (A) 启动游戏   (Y) 单游戏设置   (X) 重新扫描   (-) 上次运行日志             |
+-----------------------------------------------------------------------------+
```

### `--from-core` 链式返回体验

1. 核心退出回 `nx.rpg.nro --from-core` 时，前端读取 `config/global.json` 中的 `lastSelectedGameId`，直接从 `cache/library.json` 恢复列表并定位焦点到刚才游玩的游戏卡片。
2. 若检测到 `logs/games/<game_id>.log` 末尾存在 `[nx.rpg][FATAL]` 标记，自动在底部弹出非阻塞告警提示，按 `-` 键即可打开诊断页查看完整堆栈与修复建议（如“缺少 RM2003 RTP”、“建议将编码改为 GBK”或“请使用 Title Takeover 模式运行 MZ 游戏”）。
