# 实验 17：使用 pidfd 等待进程

## 目标

通过代表进程的文件描述符等待子进程退出，并使用 `waitid` 取得退出状态，理解 pidfd 如何避免仅靠 PID 带来的复用竞态。

## 关键 API

- `fork(2)`：创建短生命周期子进程。
- `pidfd_open(2)`：取得指向该进程的 pidfd。
- `poll(2)`：等待 pidfd 变为可读。
- `waitid(2)` 与 `P_PIDFD`：通过 pidfd 回收子进程。

## 原理

子进程以状态 42 退出。父进程通过系统调用兼容封装打开 pidfd，使用 `poll` 等待退出事件，再以 `waitid(P_PIDFD, ...)` 回收并校验 PID、退出原因和状态码。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_pidfd
./build/dev/bin/lab_pidfd
```

## 预期输出

```text
pidfd=3 became readable; child 1234 exited with status 42
```

文件描述符和 PID 每次运行可能不同。

## 注意事项

- 本实验仅适用于 Linux。
- `pidfd_open` 需要 Linux 5.3+；`waitid(P_PIDFD)` 需要 Linux 5.4+。
- 兼容函数使用 `syscall(2)`，以支持尚未声明 libc 包装函数的头文件。
- pidfd 也可加入 `epoll`，统一处理 I/O 与进程生命周期事件。

## 思考

守护进程管理大量子进程时，pidfd 相比 `SIGCHLD` 加 `waitpid` 能简化哪些竞态？
