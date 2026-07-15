# 实验 21：stdio 用户态缓冲

## 目标

把 `FILE` 流连接到管道，观察目标 libc 的全缓冲行为，并验证 `fflush` 后完整内容已经进入内核管道。

## 关键 API

- `pipe(2)`：创建作为可观察内核缓冲区的单向通道。
- `fdopen(3)`、`fileno(3)`：在文件描述符与 `FILE` 流之间建立关联。
- `setvbuf(3)`：显式配置全缓冲以及用户提供的缓冲区。
- `fflush(3)`：把待发送的 stdio 输出提交给底层文件描述符。
- `fcntl(2)`：把管道读端设为 `O_NONBLOCK`。

## 原理

程序用 `fdopen` 包装管道写端，并在任何流操作之前用 `setvbuf` 安装 128 字节全缓冲区。写入短消息后，先以非阻塞读观察有多少字节已进入管道；固定 glibc 环境通常为 0。调用 `fflush` 后，程序读取剩余内容并比对完整消息；`fileno` 同时验证流仍包装原描述符。代码也接受 libc 在刷新前提前提交部分或全部数据的合法实现策略。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_stdio_buffering
./build/dev/bin/lab_stdio_buffering
```

## 预期输出

```text
before fflush: 0 bytes; after fflush: total 16 bytes via fd 4
```

刷新前的字节数属于 libc 实现观察值，固定 glibc 环境通常为 0；描述符编号也会随运行环境变化。

## 注意事项

- `setvbuf` 必须在该流的其他操作之前调用；用户提供的缓冲区必须一直存活到 `fclose`。
- `_IOFBF` 描述缓冲模式，但 ISO C/POSIX 不保证实现绝不会提前写底层对象；本实验把刷新前字节数作为观察值，而非可移植断言。
- `fflush` 对输出流表示提交用户态缓冲，但不等于把文件内容持久化到存储介质；持久化通常还需要 `fsync`。
- `fdopen` 成功后，`FILE` 流拥有该描述符，应由 `fclose` 统一刷新并关闭，不能再重复 `close`。
- stdout 连接终端、文件或管道时可能采用不同的默认缓冲策略，因此实验显式指定 `_IOFBF`。

## 书籍对应主题

- APUE《UNIX 环境高级编程》：标准 I/O 库、缓冲和流与描述符的关系。
- TLPI《Linux/UNIX 系统编程手册》：I/O 缓冲、stdio 与内核 I/O 的分层。

这里只概述对应知识点；实验代码和说明不复制书中正文或示例。

## 思考

为什么 `fork` 前未刷新的 stdio 输出可能被父子进程各写一次，而 `_exit` 与 `exit` 又会产生不同结果？
