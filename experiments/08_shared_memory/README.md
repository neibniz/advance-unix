# 实验 08：POSIX 共享内存与进程共享信号量

## 目标

让父子进程通过同一段共享映射交换数据，并用进程共享的 POSIX 信号量建立明确的同步关系。

## 关键 API

- `shm_open`、`shm_unlink`：创建唯一命名的 POSIX 共享内存对象并删除其名字。
- `ftruncate`：把对象调整到共享结构体所需的大小。
- `mmap`、`munmap`：用 `MAP_SHARED` 建立跨进程可见的映射。
- `sem_init`、`sem_wait`、`sem_post`：以 `pshared=1` 初始化并使用进程共享信号量。
- `fork`、`waitpid`：创建子进程并确认它正常完成。

## 原理

共享内存名包含当前 PID，并配合 `O_EXCL` 避免覆盖已有对象。完成映射后程序立即 `shm_unlink`，名字消失，但映射在最后一个引用释放前仍有效。`fork` 后子进程继承映射并等待信号量；父进程写入 21 后发布信号，子进程计算 42，父进程回收子进程并验证结果。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_shared_memory
./build/dev/bin/lab_shared_memory
```

或运行 `ctest --preset dev -R '^lab_shared_memory$'`。

## 预期输出

```text
input=21 output=42 child=exited verification=ok
```

## 注意与思考

匿名信号量能否跨进程共享取决于它所在的存储是否真正共享；放在普通全局变量中并把 `pshared` 设为 1 仍然无效。虽然这些接口源自 POSIX，但 macOS 不支持 `sem_init` 创建的进程共享匿名信号量，因此本仓库只在 Linux 上生成该实验。提前 `shm_unlink` 可避免异常退出留下名字，但其他进程之后不能再按名字打开它。思考：如果父子进程需要反复双向交换数据，至少还需要几个同步状态或信号量？
