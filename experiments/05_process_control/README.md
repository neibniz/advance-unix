# 实验 05：`posix_spawnp`、管道与子进程回收

## 目标

创建带关闭执行标志的管道，用 `posix_spawnp` 启动子程序，通过文件动作捕获标准输出，并用 `waitpid` 验证退出状态。

## 关键 API

- `pipe2(O_CLOEXEC)`：在 Linux 上原子创建不会意外跨 `exec` 泄漏的管道描述符。
- `posix_spawnp`：按 `PATH` 查找并启动程序。
- `posix_spawn_file_actions_adddup2`、`addclose`：在子程序执行前重定向和关闭描述符。
- `waitpid`：回收子进程并读取终止状态。

## 原理

父进程将管道写端通过 spawn 文件动作复制到子进程的标准输出，然后启动外部 `printf`。父进程关闭自己的写端，读取到 EOF，等待子进程结束，并同时验证输出文本和退出码。非 Linux 系统使用 `pipe` 加 `fcntl(FD_CLOEXEC)` 回退，以保持本目标可构建。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_process_control
./build/dev/bin/lab_process_control
```

或运行 `ctest --preset dev -R '^lab_process_control$'`。

## 预期输出

```text
process control verified; captured: child: posix_spawnp
```

## 注意与思考

`posix_spawn*` 系列直接返回错误码，不一定设置 `errno`。父进程必须关闭管道写端，否则读取端可能永远等不到 EOF；所有成功创建的子进程也必须被回收。非 Linux 的 `pipe` + `fcntl` 回退不是原子操作；本实验是单线程程序，多线程程序若可能并发执行 `exec`，应优先使用平台提供的原子 close-on-exec 创建接口。思考：与 `fork` 后在多线程进程中执行复杂初始化相比，spawn 文件动作有什么优势？
