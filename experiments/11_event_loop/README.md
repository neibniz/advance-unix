# 实验 11：用 epoll 统一等待通知与定时器

## 目标

把用户态唤醒和单调时钟定时器都表示为文件描述符，并在一个最小 `epoll` 循环中统一处理。

## 关键 API

- `epoll_create1`、`epoll_ctl`、`epoll_wait`：创建兴趣集合、注册描述符并等待就绪事件。
- `eventfd`：提供可累加的 64 位事件计数器，常用于线程或组件间唤醒。
- `timerfd_create`、`timerfd_settime`：把定时到期转换为可读事件。
- `read`、`write`：消费计数器和定时器到期次数。

## 原理

程序把 `eventfd` 和 `timerfd` 注册到同一个 epoll 实例，用 `data.u32` 区分事件来源。一次性定时器设为 20 ms，同时向 `eventfd` 写入 7。循环不假设事件顺序，分别读取两个 64 位计数并在都出现后验证结果。

## 构建与运行

本实验只在 Linux 上生成：

```sh
cmake --preset dev
cmake --build --preset dev --target lab_event_loop
./build/dev/bin/lab_event_loop
```

或运行 `ctest --preset dev -R '^lab_event_loop$'`。

## 预期输出

这个定时器没有设置 `it_interval`，所以只到期一次：

```text
eventfd=7 timer_expirations=1 verification=ok
```

## 注意与思考

默认 epoll 是水平触发：只要描述符仍可读，就会继续报告。`eventfd` 和 `timerfd` 都要求按 8 字节计数读取；若改成周期定时器，一次读取会返回自上次读取以来的累计到期次数。`CLOCK_MONOTONIC` 不受系统墙上时间调整影响。思考：改成边沿触发后，为什么通常要把非阻塞描述符一直读到 `EAGAIN`？
