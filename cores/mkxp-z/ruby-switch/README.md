# Static Ruby MRI 3.1 Cross-Compilation for Switch (`devkitA64` / `newlib`)

`mkxp-z` 默认嵌入 **Ruby MRI 3.1**（`mri_version = '3.1'`）。在 Nintendo Switch（Horizon OS + `newlib`）上不支持 `dlopen` 动态扩展、进程 `fork/exec` 或 MJIT/YJIT，因此必须在宿主机先构建同版本 Host `miniruby`/`ruby`（作为 `--with-baseruby`），再交叉编译静态单库 `libruby-static.a`。

## 关键 `./configure` 参数基线

```bash
./configure \
  --host=aarch64-none-elf \
  --build=x86_64-pc-linux-gnu \
  --with-baseruby="$(command -v ruby)" \
  --disable-shared \
  --enable-static \
  --with-static-linked-ext \
  --with-coroutine=arm64 \
  --disable-dln \
  --disable-jit-support \
  --disable-yjit \
  --disable-install-doc \
  --disable-rubygems \
  ac_cv_func_fork=no \
  ac_cv_func_execv=no \
  ac_cv_func_pipe=no \
  ac_cv_func_getlogin=no
```

## 静态扩展裁剪策略

参见同目录 [ext-static.list](ext-static.list)：
- **必须保留**：`zlib`（RGSS 脚本表 `Scripts.rxdata`/`rvdata`/`rvdata2` 解压必需！）、`stringio`、`fiber`、`pathname`、以及完整的 CJK 字符集转码表（`shift_jis`、`gbk`、`big5`、`cp949`、`utf_16le`）。
- **明确剔除**：`fiddle`、`socket`、`openssl`、`pty`、`syslog`、`readline`、`dbm`、`gdbm`。
