# 实验 14：资源限制与资源用量

## 目标

查询进程的文件描述符限制，安全地把软限制临时设为不高于 64 后恢复，并读取进程资源用量。

## 关键 API

- `getrlimit(2)`：读取当前进程的软、硬限制。
- `prlimit(2)`：原子地查询或修改指定进程的限制。
- `getrusage(2)`：读取用户态、内核态 CPU 时间等统计量。

## 原理

程序比较 `getrlimit` 与 `prlimit` 的查询结果，把 `RLIMIT_NOFILE` 软限制临时设为原值与 64 中较小者，确认修改生效后立即恢复原值；原值已经不高于 64 时不会进一步降低。它不尝试打开大量文件，因此不会真正耗尽资源。最后校验并输出 `getrusage` 的时间字段。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_resource_limits
./build/dev/bin/lab_resource_limits
```

## 预期输出

输出随环境变化，例如：

```text
soft=1048576 hard=1048576; temporary-soft=64; user=0.000000 sys=0.001234
```

## 注意事项

- `prlimit` 是 Linux 扩展；本实验标记为 Linux-only。
- 非特权进程可降低硬限制，却通常不能再把它升回去，因此实验只修改软限制。
- 资源统计是快照，CPU 时间每次运行都可能不同。

## 思考

服务进程应如何选择 `RLIMIT_NOFILE`，并使其与事件循环和连接上限保持一致？
