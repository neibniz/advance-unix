# advance-unix

`advance-unix` 是一组用于学习 Unix/Linux 高级系统编程 API 的小实验。每个实验只有一个可执行程序，默认不需要参数，会自行创建临时资源、验证结果并清理现场。代码刻意保持短小，重点是观察 API 的语义，而不是搭建业务框架。

项目使用 C17 和现代、目标级（target-based）CMake。标准验证环境是：

- 远端：`root@192.168.31.192`
- 工作目录：`/data/advance-unix`
- 容器：`ghcr.io/neibniz/clang-dev:7ee244e55f19`
- 工具链：Clang 22、CMake 4、Ninja

## 快速开始

从本机把源码同步到远端 `/data`：

```sh
rsync -az \
  --no-owner \
  --no-group \
  --exclude build \
  -e 'ssh -i ~/.ssh/ssh_linkwater' \
  ./ root@192.168.31.192:/data/advance-unix/
```

登录并在指定镜像中完成配置、编译和全部测试：

```sh
ssh root@192.168.31.192 -i ~/.ssh/ssh_linkwater
cd /data/advance-unix
./scripts/container-build.sh
```

脚本拒绝挂载 `/data` 以外的项目目录，并以 `--network=none` 启动容器。网络实验只使用容器自己的 loopback。

单独运行一个实验时，也使用同一个容器：

```sh
docker run --rm \
  --pull=never \
  --network=none \
  --mount type=bind,src="$PWD",dst=/workspace \
  --workdir /workspace \
  ghcr.io/neibniz/clang-dev:7ee244e55f19 \
  ./build/dev/bin/lab_event_loop
```

下面的 CMake preset 命令均应在 [docs/ENVIRONMENT.md](docs/ENVIRONMENT.md) 所示的手工容器内执行：

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

需要检查内存错误时，可在容器中使用 `sanitizers` preset：

```sh
cmake --preset sanitizers
cmake --build --preset sanitizers
ctest --preset sanitizers
```

## 实验索引

| # | 子系统 | 关键 API | 可移植性 |
|---:|---|---|---|
| 01 | 高级文件 I/O | `pread`、`pwrite`、`readv`、`writev` | POSIX |
| 02 | 目录与路径 | `openat`、`fstatat`、`renameat`、`unlinkat` | POSIX |
| 03 | 虚拟内存 | `mmap`、`msync`、`munmap` | POSIX |
| 04 | 文件锁 | `fcntl`、`F_OFD_SETLK` | Linux |
| 05 | 进程创建 | `posix_spawnp`、file actions、`waitpid` | POSIX（`pipe2` 为 Linux） |
| 06 | 同步信号 | `sigprocmask`、`signalfd` | Linux |
| 07 | 线程同步 | `pthread_mutex_*`、`pthread_cond_*` | POSIX |
| 08 | 共享内存 | `shm_open`、共享 `mmap`、`sem_init` | POSIX |
| 09 | Unix 域套接字 | `socketpair`、`SCM_RIGHTS` | Unix/Linux |
| 10 | TCP 套接字 | `getaddrinfo`、非阻塞 `connect`、`poll` | POSIX/Linux |
| 11 | 事件循环 | `epoll`、`eventfd`、`timerfd` | Linux |
| 12 | 文件系统通知 | `inotify` | Linux |
| 13 | 零拷贝 | `splice` | Linux |
| 14 | 资源核算 | `getrlimit`、`prlimit`、`getrusage` | POSIX/Linux |
| 15 | CPU 调度 | `sched_getaffinity`、`sched_setaffinity` | Linux |
| 16 | 终端与 PTY | `posix_openpt`、`grantpt`、`termios` | POSIX/XSI |
| 17 | 进程描述符 | `pidfd_open`、`poll`、`waitid` | Linux |
| 18 | POSIX 消息队列 | `mq_open`、`mq_send`、`mq_receive` | POSIX API（本项目仅在 Linux 构建） |

每一行都对应 `experiments/NN_name/README.md`，其中包含原理、预期输出、边界条件和思考题。

## 建议学习顺序

1. 先做 01–03，建立“文件描述符、打开文件描述、地址空间”的区别。
2. 再做 05–09，理解进程、信号、线程与 IPC 的生命周期。
3. 然后做 10–13，观察非阻塞 I/O、就绪通知和数据路径。
4. 最后做 14–18，学习资源控制、调度以及 Linux 的现代接口。

进一步的实验方法、可移植性说明和资料索引见 [docs/LEARNING_GUIDE.md](docs/LEARNING_GUIDE.md)，环境约束见 [docs/ENVIRONMENT.md](docs/ENVIRONMENT.md)。

## 设计原则

- 一个目录只讲一个核心概念，程序默认可重复运行。
- 关键系统调用都检查返回值；主要失败路径打印 API 名和 `errno` 文本。
- POSIX 接口与 Linux 专有接口在文档和 CMake 中明确区分。
- 测试只使用临时文件、匿名/唯一命名 IPC 对象和 loopback，不依赖外网。
- 示例用于学习语义，不直接等同于生产级封装。
