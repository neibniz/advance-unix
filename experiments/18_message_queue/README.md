# 实验 18：POSIX 消息队列

## 目标

创建唯一命名的 POSIX 消息队列，发送两个不同优先级的消息，验证接收顺序并可靠清理队列名称。

## 关键 API

- `mq_open(3)`、`mq_close(3)`、`mq_unlink(3)`：管理队列生命周期。
- `mq_send(3)`、`mq_receive(3)`：发送和接收带优先级的消息。
- `mq_getattr(3)`：读取队列容量、消息大小和当前消息数。

## 原理

队列以 PID 和单调时钟纳秒字段生成唯一名称，并用 `O_EXCL` 防止误用旧对象。程序先发送低优先级消息，再发送高优先级消息；POSIX 消息队列应先返回最高优先级消息。内容、长度、优先级和队列属性都会被校验。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_message_queue
./build/dev/bin/lab_message_queue
```

## 预期输出

```text
queue /advance_unix_1234_567890 delivered priorities 7 then 1 and was cleaned up
```

队列名称每次运行不同。

## 注意事项

- 内核需要启用 POSIX 消息队列，并受 IPC namespace、`RLIMIT_MSGQUEUE` 和 `/proc/sys/fs/mqueue/*` 限制。挂载 `/dev/mqueue` 只是在文件系统中查看/管理队列的方式，不是 `mq_open` 的必要条件。
- 队列名称以 `/` 开头，但不能再包含其他 `/`。
- 消息队列对象可能在进程退出后继续存在，因此所有路径都应调用 `mq_unlink`。
- API 属于 POSIX；本仓库为保持固定镜像和 `librt` 链接方式一致，只在 Linux 上生成该目标。其他 Unix-like 系统可能需要不同的可用性检测或链接选项。

## 思考

消息优先级可能造成低优先级消息饥饿；生产系统应如何限制或调度优先级？
