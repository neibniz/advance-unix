# 实验 02：基于目录描述符的文件系统操作

## 目标

用目录文件描述符作为稳定锚点完成创建、查询、重命名和删除，并观察 `O_NOFOLLOW` 对最终路径分量中符号链接的保护。

## 关键 API

- `openat`：相对于目录描述符打开文件。
- `fstatat`：相对于目录描述符读取元数据。
- `renameat`、`unlinkat`：相对路径重命名和删除。
- `O_NOFOLLOW`：拒绝跟随路径的最终符号链接。

## 原理

程序创建临时目录并打开其目录描述符。后续文件操作只使用相对名称和 `dirfd`：创建普通文件、检查类型和大小、创建符号链接并确认 `O_NOFOLLOW` 令打开失败，最后将文件重命名并删除。

## 原理插图

![实验 02：基于目录描述符的文件系统操作原理插图](https://oss.euler.icu/teaser/advance-unix/principles/02_filesystem.png?v=320c15c7)

> 蓝色表示用户空间，琥珀色表示内核对象，绿色表示成功路径，珊瑚色表示语义边界或失败路径。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_filesystem
./build/dev/bin/lab_filesystem
```

或运行 `ctest --preset dev -R '^lab_filesystem$'`。

## 预期输出

```text
filesystem APIs verified: regular file renamed and cleaned
```

## 注意与思考

临时目录建立在当前工作目录，程序会删除其中资源。`O_NOFOLLOW` 只约束最终路径分量；若要抵御不可信多级路径中的链接替换，可继续研究 Linux `openat2` 的解析限制。思考：长时间持有可信目录描述符，为什么能减少工作目录变化和路径竞争带来的问题？
