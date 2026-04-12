# aslr_report

只读的 ASLR 诊断模块，用来查看内核里和 ASLR 相关的符号解析结果与当前 `randomize_va_space` 值。

这个模块不会关闭 ASLR，也不会修改 `load_elf_binary`、`personality`、linker 随机化逻辑。它的作用仅限于：

- 解析 `randomize_va_space`
- 解析 `load_elf_binary` / `load_elf_binary.cfi_jt`
- 通过 `ctl0` 返回当前状态

## ctl0

支持的命令：

- `status`
- `refresh`
- `source`

## 来源

原始思路来源于下面这篇文章；本文仓库里的实现仅保留“状态观测”部分，不实现“关闭 ASLR / 固定随机因子”能力：

- https://mp.weixin.qq.com/s/N7oaErkBhYaQADxv-DAiNg
