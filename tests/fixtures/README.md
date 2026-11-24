# 零版权合成引擎测试夹具规范（Synthetic Fixtures）

严禁向本目录提交任何商业游戏文件或官方 RTP 素材。Phase 1 单测使用脚本或最小合法字节头动态构造以下 7 类空壳工程夹具：

| 夹具名称 | 最小特征文件构造 | 预期检测结果 |
| :--- | :--- | :--- |
| `01_rmmz_basic` | `index.html` + `js/rmmz_core.js` + `data/System.json` (`{"gameTitle":"MZ Test"}`) | `engine=rmmz`, `core=nxjs-mvz`, `rgssVersion=0` |
| `02_rmmv_desktop_www` | `Game.exe`（空占位）+ `www/js/rpg_core.js` + `www/data/System.json` | `engine=rmmv`, `core=nxjs-mvz`, `contentRoot=<dir>/www` |
| `03_rmvxa_renamed_archive` | `Touhou.ini` (`[Game]\r\nLibrary=System\\RGSS301.dll\r\nTitle=テスト`) + `Touhou.rgss3a` | `engine=rmvxa`, `core=mkxp-z`, `rgssVersion=3`, `execName=Touhou` |
| `04_rmvx_standard` | `Game.ini` (`Library=RGSS202E.dll`) + `Data/Scripts.rvdata` | `engine=rmvx`, `core=mkxp-z`, `rgssVersion=2` |
| `05_rmxp_gbk_title` | GBK 编码 `Game.ini` + `Game.rgssad`（含 `RGSSAD\0\x01` 8 字节魔数头） | `engine=rmxp`, `core=mkxp-z`, `rgssVersion=1` |
| `06_rm2k3_ldb` | 含 `LcfDataBase` 头部与 2003 特征字段的最小 `RPG_RT.ldb` + `RPG_RT.lmt` + `Battle2/` | `engine=rm2k3`, `core=easyrpg` |
| `07_rm2k_ldb` | 含 `LcfDataBase` 头部的最小 `RPG_RT.ldb` + `RPG_RT.lmt` | `engine=rm2k`, `core=easyrpg` |
