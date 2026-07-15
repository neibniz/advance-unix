# 实验 26：System V 消息队列

## 目标

创建私有 System V 消息队列，让子进程发送带类型的消息，父进程检查队列属性并按类型接收，最后显式删除内核对象。

## 书籍对应主题

- **APUE**：System V IPC、消息队列及 IPC 权限结构。
- **TLPI**：System V 消息队列的创建、传输、控制与删除。

## 关键 API

- `msgget(2)`：用 `IPC_PRIVATE` 创建全新的队列。
- `msgsnd(2)`、`msgrcv(2)`：发送消息并按正类型选择消息。
- `msgctl(2)`：用 `IPC_STAT` 查看队列，用 `IPC_RMID` 删除队列。
- `fork(2)`、`waitpid(2)`：让子进程发送并由父进程回收。

## 原理

`IPC_PRIVATE` 每次都创建新队列，返回的标识符在 `fork` 后由父子进程共享。消息结构的第一个字段必须是正 `long` 类型；传给 `msgsnd`/`msgrcv` 的长度只计算正文，不包含该字段。子进程使用 `IPC_NOWAIT` 发送后退出，父进程先回收并检查退出状态，再通过 `IPC_STAT` 确认队列内恰有一条消息，按类型非阻塞接收并验证内容。所有父进程路径最终执行 `IPC_RMID`。

## 构建与运行

本实验在仓库中固定为 Linux 目标：

```sh
cmake --preset dev
cmake --build --preset dev --target lab_sysv_message_queue
./build/dev/bin/lab_sysv_message_queue
```

也可以运行 `ctest --preset dev -R '^lab_sysv_message_queue$'`。

## 预期输出

队列标识符随运行变化：

```text
queue id=42 delivered type=7 and was removed
```

## 注意与思考

System V IPC 对象属于内核而非文件系统，最后一个进程退出后仍可继续存在，因此错误路径也必须删除队列。接口有悠久的跨 Unix 历史，但各平台的限制、工具和结构细节不同；本实验按固定 Linux 环境构建。思考：与 POSIX 消息队列相比，类型选择和优先级语义有何差异？
