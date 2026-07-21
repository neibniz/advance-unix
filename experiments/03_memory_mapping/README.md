# 实验 03：共享文件映射与同步

## 目标

建立可写的文件共享映射，通过普通内存访问修改文件，并显式同步后从文件描述符读回验证。

## 关键 API

- `ftruncate`：把空文件扩展到一个系统页。
- `mmap`：以 `MAP_SHARED` 建立读写映射。
- `msync`：使用 `MS_SYNC` 等待脏页同步完成。
- `munmap`：解除虚拟地址映射。

## 原理

文件映射要求底层文件覆盖将要访问的范围，因此先以 `ftruncate` 设置长度。`MAP_SHARED` 使映射中的修改对文件可见；写入字符串并执行 `msync` 后，程序解除映射，再用 `pread` 从文件读取相同内容完成自验证。这个读回步骤验证可见性，而 `msync(MS_SYNC)` 的返回值验证同步请求是否成功。

## 原理插图

![实验 03：共享文件映射与同步原理插图](https://oss.euler.icu/teaser/advance-unix/principles/03_memory_mapping.png?v=6bdd7569)

> 蓝色表示用户空间，琥珀色表示内核对象，绿色表示成功路径，珊瑚色表示语义边界或失败路径。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_memory_mapping
./build/dev/bin/lab_memory_mapping
```

或运行 `ctest --preset dev -R '^lab_memory_mapping$'`。

## 预期输出

页大小通常为 4096 字节，实际数字以系统为准：

```text
memory mapping verified: visible through MAP_SHARED (4096-byte mapping)
```

## 注意与思考

访问超过文件长度的映射区域可能触发 `SIGBUS`。解除映射后立刻 `pread` 仍可能命中同一页缓存，因此本实验不能证明物理介质上的掉电持久性；研究持久化还要结合 `fsync`、存储设备与具体文件系统语义。思考：若改用 `MAP_PRIVATE`，本实验最后的文件内容会发生什么变化？
