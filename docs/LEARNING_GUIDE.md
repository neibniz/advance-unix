# 学习与实验指南

## 先建立三个概念

1. **文件描述符**是进程表中的整数槽位；`dup` 后多个描述符可以引用同一个打开文件描述。
2. **打开文件描述**保存当前偏移、状态标志等内核状态；这解释了 `pread` 与普通 `read`、OFD 锁与传统进程锁的差异。
3. **就绪不等于完成**。`poll`、`epoll` 和 `signalfd` 告诉程序“现在操作大概率不会阻塞”，仍然必须处理短读、短写、`EINTR` 和 `EAGAIN`。

## POSIX 与 Linux 专有接口

POSIX 实验展示跨 Unix-like 系统通常共有的抽象；Linux 实验展示生产环境常用但不可直接移植的能力。

| 层次 | 本项目示例 | 学习重点 |
|---|---|---|
| ISO C | 基本数据与错误输出 | 语言层，不描述进程或文件描述符 |
| POSIX | `pthread`、`mmap`、`shm_open`、`mq_open`、`aio_read` | Unix-like 系统的共同接口 |
| XSI | PTY 相关接口 | POSIX 的扩展选项组 |
| System V | `msgget`、`msgsnd`、`msgrcv` | 历史悠久、仍广泛存在的 IPC 接口 |
| Linux | `epoll`、`signalfd`、`pidfd`、`inotify`、`memfd_create` | 内核专有、高性能或新式生命周期接口 |

CMake 中的 `LINUX_ONLY` 会在非 Linux 平台跳过对应目标。源码使用 `_GNU_SOURCE` 暴露 glibc 的 Linux/GNU 声明。

## 推荐观察方法

每个实验可按同一循环学习：

1. 先读目录 README，预测输出和内核对象的生命周期。
2. 只读 `main.c`，画出文件描述符或进程之间的关系。
3. 运行程序，确认自验证结果。
4. 若镜像包含 `strace`，用 `strace -f ./build/dev/bin/<目标>` 对照系统调用。
5. 修改一个标志或故意跳过一次清理，观察错误码，再恢复代码。

常见错误码值得主动实验：

- `EINTR`：阻塞调用被信号中断；
- `EAGAIN`/`EWOULDBLOCK`：非阻塞资源暂时不可用；
- `EBADF`：描述符无效或访问模式错误；
- `EPIPE`：管道/套接字的读端已经关闭；
- `ETIMEDOUT`：等待超过约定时间。

## 如何阅读手册

系统调用通常位于手册第 2 节，库函数位于第 3 节：

```sh
man 2 openat
man 2 epoll_wait
man 3 pthread_cond_wait
man 7 signal
man 7 unix
```

阅读时依次看 `SYNOPSIS`、返回值、`ERRORS`、线程安全属性与版本/标准说明。不要只复制函数签名；高级 API 的难点通常在对象所有权、并发时序和失败恢复。

## 延伸资料

- W. Richard Stevens、Stephen A. Rago，[《UNIX 环境高级编程》（APUE）第 3 版](https://www.informit.com/store/advanced-programming-in-the-unix-environment-9780321638007)；
- Michael Kerrisk，[《Linux/UNIX 系统编程手册》（TLPI）及详细目录](https://man7.org/tlpi/toc-detailed.html)；
- Linux `man-pages` 项目，尤其第 2、3、7 节；
- The Open Group 的 POSIX Base Specifications；
- Linux 内核文档中与文件系统、调度、网络和 userspace API 相关的章节。

书和标准用于理解稳定语义，当前运行内核的 `man` 页面用于确认 Linux 专有标志、版本要求和错误码。完整的主题映射和仍未覆盖的领域见 [BOOK_COVERAGE.md](BOOK_COVERAGE.md)。

## 增加一个实验

新实验保持三件套：`main.c`、`README.md`、`CMakeLists.txt`。CMake 目标应使用项目 helper，而不是全局编译选项：

```cmake
add_unix_experiment(lab_example
  SOURCES main.c
  LINUX_ONLY
  LIBRARIES Threads::Threads
)
```

然后把目录显式加入 `experiments/CMakeLists.txt`，运行 `cmake --workflow --preset verify`。若实验依赖权限、特殊挂载或外部服务，不应默认注册为 CTest；需要在 README 中明确前置条件。
