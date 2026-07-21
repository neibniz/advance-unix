# 实验 25：命名管道 FIFO

## 目标

在临时目录创建 FIFO，由子进程写入、父进程通过非阻塞读端和 `poll` 接收数据，并在所有路径清理文件系统对象。

## 书籍对应主题

- **APUE**：进程间通信、管道与 FIFO。
- **TLPI**：FIFO、非阻塞 I/O 与 I/O 多路复用。

## 关键 API

- `mkdtemp(3)`、`mkfifo(3)`：创建唯一目录和命名管道。
- `open(2)`：以 `O_NONBLOCK` 打开读端和写端，避免双方等待。
- `poll(2)`：限时等待读端就绪，并处理挂断状态。
- `fork(2)`、`waitpid(2)`：运行并回收写入者。
- `unlink(2)`、`rmdir(2)`：清理 FIFO 及其目录。

## 原理

父进程先以非阻塞方式打开 FIFO 读端，因此不会等待写入者。子进程关闭继承的读端，再以非阻塞方式打开写端，一次写入小于 `PIPE_BUF` 的消息并退出。父进程先检查子进程状态，保证此时要么已有完整消息、要么已得到明确错误，再用带超时的 `poll` 等待并读取，从而避免 `open`/`read` 时序导致永久阻塞。FIFO 是字节流，接收端仍按已知长度循环读取。

## 原理插图

![实验 25：命名管道 FIFO 原理插图](https://oss.euler.icu/teaser/advance-unix/principles/25_fifo.png)

> 蓝色表示用户空间，琥珀色表示内核对象，绿色表示成功路径，珊瑚色表示语义边界或失败路径。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_fifo
./build/dev/bin/lab_fifo
```

也可以运行 `ctest --preset dev -R '^lab_fifo$'`。

## 预期输出

```text
FIFO delivered 27 bytes through poll and was cleaned up
```

## 注意与思考

FIFO 节点会保留在文件系统中，程序退出不会自动删除它。本实验把临时目录建在当前工作目录；远端运行时工作目录位于 `/data`，不会触碰远端其他目录。多个写入者只有在单次写入不超过 `PIPE_BUF` 时才能依赖原子写入。思考：未知长度协议应如何设计消息边界？
