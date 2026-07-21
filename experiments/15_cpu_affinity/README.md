# 实验 15：CPU 亲和性

## 目标

读取容器允许当前线程使用的 CPU 集合，把调用线程临时绑定到其中一个 CPU，验证后恢复原集合。

## 关键 API

- `sched_getaffinity(2)`：查询线程的 CPU 亲和性掩码；PID 为 0 表示调用线程。
- `sched_setaffinity(2)`：修改指定线程的 CPU 亲和性掩码。
- `CPU_ZERO`、`CPU_SET`、`CPU_ISSET`、`CPU_COUNT`：操作 CPU 集合。

## 原理

程序不会假定 CPU 0 可用，而是从调用线程当前的允许集合中选择第一个 CPU。绑定后再次读取亲和性并确认集合只有该 CPU，随后恢复最初的掩码并进行第二次校验。这个实验只有一个线程；多线程进程的亲和性需要逐线程设置。

## 原理插图

![实验 15：CPU 亲和性原理插图](https://oss.euler.icu/teaser/advance-unix/principles/15_cpu_affinity.png)

> 蓝色表示用户空间，琥珀色表示内核对象，绿色表示成功路径，珊瑚色表示语义边界或失败路径。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_cpu_affinity
./build/dev/bin/lab_cpu_affinity
```

## 预期输出

数值取决于容器的 cpuset 配置，例如：

```text
allowed CPUs=8; temporarily selected CPU=2; restored=yes
```

## 注意事项

- 这是 Linux 专用接口。
- Docker/cgroup 可能只允许宿主机 CPU 的一个子集，不能假定编号连续。
- 固定大小的 `cpu_set_t` 最多表示 `CPU_SETSIZE` 个 CPU；超大系统应使用 `CPU_ALLOC` 动态集合。
- 亲和性只是允许运行位置；设置后不保证调用线程立即迁移。
- pthread 程序可用非标准的 `pthread_setaffinity_np(3)` 明确指定某个线程。

## 思考

线程池应按逻辑 CPU、物理核心还是 NUMA 节点分配亲和性？
