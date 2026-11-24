# third_party — 上游核心锁定清单与源码挂载目录

本目录通过 [upstreams.lock.json](upstreams.lock.json)（Schema 见 [upstreams-lock.schema.json](../common/schemas/upstreams-lock.schema.json)）精确锁定三大上游核心仓库的 Git URL、分支与 **40 位 Commit SHA**。实际源码目录（`EasyRPG-Player/`、`nx.js/`、`mkxp-z/`）被 `.gitignore` 忽略，不提交至主仓库。

## 常用操作

```bash
# 按 upstreams.lock.json 检出精确锁定的上游 Commit
bash scripts/bootstrap-cores.sh

# 对已检出的上游目录检查或应用 cores/<core>/patches/SERIES 中的补丁队列
bash scripts/apply-patches.sh --check all
```
