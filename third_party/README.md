# third_party — 上游核心源码挂载目录

本目录用于存放通过脚本拉取的三大上游核心仓库（默认被 `.gitignore` 忽略，不直接提交庞大上游代码至主仓库）：

- `third_party/EasyRPG-Player` → `https://github.com/EasyRPG/Player`
- `third_party/nx.js` → `https://github.com/TooTallNate/nx.js`
- `third_party/mkxp-z` → `https://github.com/mkxp-z/mkxp-z`

运行仓库根目录下的脚本一键初始化或更新：

```bash
./scripts/bootstrap-cores.sh
```
