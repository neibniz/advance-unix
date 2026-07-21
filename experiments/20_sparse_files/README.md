# 实验 20：稀疏文件与数据区间

## 目标

用定位写入制造约 8 MiB 的文件空洞，比较逻辑大小与实际分配块数，并通过 Linux 的 `SEEK_DATA`、`SEEK_HOLE` 查询数据和空洞区间。

## 关键 API

- `pwrite(2)`：在指定偏移写入，不改变共享文件偏移量。
- `fstat(2)`：读取 `st_size` 和 `st_blocks`。
- `lseek(2)` 配合 `SEEK_DATA`、`SEEK_HOLE`：查询下一个数据区或空洞。

## 原理

程序在偏移 0 写入 `HEAD`，再在 8 MiB 偏移处写入 `TAIL`。中间未写入的范围构成空洞，因此 `st_size` 约为 8 MiB，而 `st_blocks * 512` 明显更小。随后程序验证第一个数据区、空洞和第二个数据区的相对位置。`pwrite` 使用循环处理短写与 `EINTR`，所有退出路径都会关闭并删除临时文件。

## 原理插图

![实验 20：稀疏文件与数据区间原理插图](https://oss.euler.icu/teaser/advance-unix/principles/20_sparse_files.png)

> 蓝色表示用户空间，琥珀色表示内核对象，绿色表示成功路径，珊瑚色表示语义边界或失败路径。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_sparse_files
./build/dev/bin/lab_sparse_files
```

## 预期输出

分配字节数和第一个数据区终点取决于文件系统，例如：

```text
logical=8388612 allocated=8192; extents data[0,4096) data[8388608,8388612)
```

若底层文件系统不实现详细 extent 查询，或采用“所有偏移均为数据、仅 EOF 为 hole”的合法最小实现，程序会明确输出 `filesystem does not support detailed SEEK_DATA/SEEK_HOLE, extent check skipped`；固定开发容器及 `/data` 文件系统应走完整验证路径。

## 注意事项

- 本实验使用 Linux 接口并标记为 Linux-only；并非所有 Unix 或文件系统都支持 `SEEK_DATA`/`SEEK_HOLE`。
- `st_blocks` 按 512 字节单位报告，不等于文件系统自身的块大小。
- extent 的边界通常按分配块对齐，因此第一个数据区可能大于实际写入的 4 字节。
- 标准允许文件系统给出保守的粗粒度答案；`SEEK_DATA`/`SEEK_HOLE` 不能当作物理 extent 的精确查询接口。
- 读取空洞会得到零字节，但备份、复制或改写工具若不了解稀疏布局，可能把空洞实体化。

## 书籍对应主题

- APUE《UNIX 环境高级编程》：文件 I/O、文件偏移与文件空洞。
- TLPI《Linux/UNIX 系统编程手册》：通用 I/O 模型、稀疏文件和 `lseek` 扩展。

这里只概述对应知识点；实验代码和说明不复制书中正文或示例。

## 思考

复制稀疏虚拟机镜像时，怎样同时保持数据内容、空洞布局和崩溃后的一致性？
