# 实验 06：把信号转换为文件描述符事件

## 目标

阻塞传统信号投递，再通过 Linux `signalfd` 同步读取信号及发送者元数据，避免在异步信号处理函数中工作。

## 关键 API

- `sigprocmask`：阻塞 `SIGUSR1` 并在结束时恢复原掩码。
- `signalfd`：把掩码中的待处理信号暴露为可读文件描述符。
- `kill`：向当前进程发送测试信号。
- `read`：读取 `signalfd_siginfo` 元数据。

## 原理

要交给 `signalfd` 的信号必须先被阻塞，否则仍可能按传统方式递送。程序阻塞 `SIGUSR1`、创建信号描述符并给自己发送信号；随后读取并校验信号编号和发送进程 PID，关闭描述符，最后恢复进入程序前的信号掩码。

## 构建与运行

本实验只在 Linux 上生成目标：

```sh
cmake --preset dev
cmake --build --preset dev --target lab_signals
./build/dev/bin/lab_signals
```

或运行 `ctest --preset dev -R '^lab_signals$'`。

## 预期输出

PID 会随运行改变：

```text
signal fd verified: SIGUSR1 from pid 1234
```

## 注意与思考

在多线程程序中，通常应在创建线程前阻塞目标信号，让所有线程继承掩码；否则信号仍可能递送到未阻塞它的线程。`signalfd` 是 Linux 专有接口。思考：它如何与 `poll`、`epoll` 中的套接字和定时器事件统一处理？
