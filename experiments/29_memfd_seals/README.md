# 实验 29：memfd 文件封印

## 目标

创建一个匿名内存文件，写入内容后添加写入、增长、缩小和封印锁定四种 seal，并验证后续修改及调整大小都以 `EPERM` 失败。

## 书籍主题

- APUE：文件描述符、匿名临时对象与进程间资源共享。
- TLPI：内存映射、共享内存、`fcntl` 控制操作；`memfd` 和 seals 是这些主题上的 Linux 后续扩展。

## 关键 API

- `memfd_create(2)`：创建只存在于内存中的匿名文件描述符。
- `MFD_ALLOW_SEALING`：允许随后为 memfd 添加 seals。
- `fcntl(2)` 的 `F_ADD_SEALS`：单向增加文件约束。
- `fcntl(2)` 的 `F_GET_SEALS`：查询当前 seal 位集合。
- `pwrite(2)`、`ftruncate(2)`：验证写入和尺寸变化已被禁止。

## 原理

程序先写入一段文本，再添加 `F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK`，最后以 `F_SEAL_SEAL` 禁止继续修改 seal 集合。程序再次调用 `F_ADD_SEALS`，必须得到 `EPERM`；读取仍然允许，但覆盖原内容、增长和缩小文件也都必须失败。seal 只能增加，不能移除。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_memfd_seals
./build/dev/bin/lab_memfd_seals
```

## 预期输出

```text
memfd seals 0xf blocked write, grow, and shrink
```

## 平台边界

`memfd_create` 与文件 seals 是 Linux 专用接口，目标因此为 Linux-only。`memfd_create` 自 Linux 3.17 起可用；容器还可能通过 seccomp 限制该系统调用。输出中的 seal 数值以当前 Linux UAPI 定义为准。

## 注意事项

- 忘记传入 `MFD_ALLOW_SEALING` 时，初始状态会阻止添加 seal。
- 存在可写共享映射时，添加 `F_SEAL_WRITE` 可能以 `EBUSY` 失败。
- `F_SEAL_SEAL` 应最后添加，否则后续 seal 无法再加入。

## 思考

通过 Unix 域套接字传递已封印的 memfd，如何让接收进程验证自己拿到的是不可变数据？
