# Launcher 文件传输模块

`ftpd.*`、`mtp_core.*` 和 `mtp_usb.*` 源自 [rpynx 的传输模块](https://github.com/P4rfa1t/rpynx/tree/9c6653bdaf2133a636d165ca3704379ba5247e6c/platform/transfer)，保留 [MIT 许可](LICENSE.rpynx)。本项目使用相同的 MTP 协议核心与 USB 适配，FTP 改为监听当前本地 IPv4 地址、匿名登录（任意用户名，不校验密码），并拒绝覆盖既有上传目标。

两个服务均由 Launcher 的「文件传输」页通过开关手动启停，可同时开启；切换页签后仍在后台运行，直到关闭开关、启动游戏前确认停止或退出 Launcher。FTP 的 `/` 和 MTP 的存储根均映射到 Launcher 的 `switch/nx.rpg/`，包括 `games/`、`config/`、`saves/`、`logs/` 和 `cores/`，不开放该目录之外的 SD 卡内容。上传先写同目录临时文件，同步后再改名，传输中的文件名与速率由 `rsl_ftpd_status` 提供给「文件传输」页。FTP 为明文、仅支持被动模式和单客户端，不应在不可信网络开启。MTP 依赖 Switch 原生 USB 设备服务，应仅连接可信电脑。
