# 实验 23：可移植的信号等待

## 目标

用 POSIX 信号接口安全等待 `SIGUSR1`，验证“先阻塞、后用 `sigsuspend` 原子解除阻塞并等待”不会丢失已经到达的信号。

## 书籍对应主题

- **APUE**：信号、可靠信号术语、信号集与 `sigsuspend`。
- **TLPI**：信号处理器、信号掩码、等待信号。

## 关键 API

- `sigaction(2)`：安装处理器并保存旧动作。
- `sigprocmask(2)`、`sigpending(2)`：阻塞信号并检查未决集合。
- `sigsuspend(2)`：临时替换掩码并原子等待信号。
- `fork(2)`、`kill(2)`、`waitpid(2)`：由子进程发送测试信号并回收子进程。

## 原理

父进程先阻塞 `SIGUSR1`，再安装处理器并 `fork`，使测试处理器不会在准备完成前运行。子进程立即发送信号并退出，父进程先回收子进程，再确认信号已经处于未决状态且处理器尚未运行。随后 `sigsuspend` 使用临时掩码解除对 `SIGUSR1` 的阻塞；内核在同一个原子操作中切换掩码并休眠，因此不存在“检查条件后、进入休眠前”丢失唤醒的窗口。处理器只写 `volatile sig_atomic_t` 标志，不调用非异步信号安全函数。

## 原理插图

![实验 23：可移植的信号等待原理插图](https://oss.euler.icu/teaser/advance-unix/principles/23_portable_signals.png)

> 蓝色表示用户空间，琥珀色表示内核对象，绿色表示成功路径，珊瑚色表示语义边界或失败路径。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_portable_signals
./build/dev/bin/lab_portable_signals
```

也可以运行 `ctest --preset dev -R '^lab_portable_signals$'`。

## 预期输出

```text
SIGUSR1 was pending, then sigsuspend delivered it atomically
```

## 注意与思考

信号处理器内可调用的函数非常有限；通常只设置标志或写入预先打开的管道。普通信号不会排队，同种信号多次到达可能只保留一个未决实例。思考：多线程程序应使用 `pthread_sigmask` 后集中在哪个线程等待信号？
