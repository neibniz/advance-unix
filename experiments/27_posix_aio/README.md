# 实验 27：POSIX 异步文件 I/O

## 目标

从临时文件提交一次异步读，等待请求结束并校验内容；同时理解异步请求在成功、失败和取消路径上都必须由 `aio_return` 回收。

## 书籍主题

- APUE：高级 I/O、POSIX 异步 I/O、可重入与错误处理。
- TLPI：I/O 模型、异步通知以及 Linux 上不同异步 I/O 接口的边界。

## 关键 API

- `aio_read(3)`：提交由 `struct aiocb` 描述的定位读。
- `aio_error(3)`：检查请求是仍在执行、成功还是以错误结束。
- `aio_suspend(3)`：阻塞等待一个或多个异步请求发生状态变化。
- `aio_return(3)`：取得最终字节数并回收请求资源，只能调用一次。
- `aio_cancel(3)`：在清理路径尝试取消请求；不能取消时仍须等待并回收。

## 原理

程序创建并立即取消链接一个临时文件，写入固定文本后以偏移 0 提交 `aio_read`。等待循环用 `aio_error` 排除虚假唤醒，再由 `aio_return` 取得结果。任何后续步骤失败时，清理路径都会先尝试取消、等待请求离开 `EINPROGRESS`，然后回收 `aiocb`。

## 原理插图

![实验 27：POSIX 异步文件 I/O 原理插图](https://oss.euler.icu/teaser/advance-unix/principles/27_posix_aio.png)

> 蓝色表示用户空间，琥珀色表示内核对象，绿色表示成功路径，珊瑚色表示语义边界或失败路径。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_posix_aio
./build/dev/bin/lab_posix_aio
```

## 预期输出

```text
POSIX AIO read 28 bytes; content matched
```

## 平台边界

POSIX AIO 是标准接口，但实现策略并不等于 Linux 内核 native AIO。glibc 对普通文件的 POSIX AIO 通常以用户态线程实现；Linux native AIO 主要通过 `io_setup/io_submit` 等系统调用服务特定文件 I/O，而现代高吞吐程序通常优先评估 `io_uring`。本仓库为固定 Linux 镜像及 `librt` 链接方式，将目标标为 Linux-only。

## 注意事项

- `aiocb`、缓冲区和文件描述符在 `aio_return` 前都必须保持有效。
- `aio_suspend` 返回只表示应重新检查状态，不保证请求成功。
- `aio_cancel` 返回“无法取消”并不代表请求丢失；调用者仍要等待和回收。

## 思考

如果同时提交多个请求，怎样组织 `aiocb` 的所有权，才能避免缓冲区过早释放或重复调用 `aio_return`？
