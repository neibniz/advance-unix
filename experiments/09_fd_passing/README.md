# 实验 09：通过 Unix 域套接字传递文件描述符

## 目标

通过 Unix 域套接字的辅助数据把一个进程中的有效文件描述符传给另一个进程，并验证接收端可以直接使用它。

## 关键 API

- `socketpair`：创建一对已连接的 Unix 域套接字。
- `sendmsg`、`recvmsg`：发送和接收带辅助数据的消息；可用时接收使用 `MSG_CMSG_CLOEXEC`。
- `SCM_RIGHTS`：请求内核在接收进程中安装文件描述符。
- `CMSG_SPACE`、`CMSG_LEN`、`CMSG_FIRSTHDR`：正确分配、填写和遍历控制消息。
- `pipe`：提供一个无需临时文件的可读描述符作为传递对象。

## 原理

程序先建立套接字对并 `fork`，然后父进程才创建管道，确保子进程不可能通过继承得到该管道。父进程向管道写入测试文本，再把读端放入 `SCM_RIGHTS` 控制消息；子进程用 `recvmsg` 得到一个新的描述符编号，读取文本并以退出状态报告验证结果。两个描述符引用同一个内核打开对象。

## 原理插图

![实验 09：通过 Unix 域套接字传递文件描述符原理插图](https://oss.euler.icu/teaser/advance-unix/principles/09_fd_passing.png)

> 蓝色表示用户空间，琥珀色表示内核对象，绿色表示成功路径，珊瑚色表示语义边界或失败路径。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_fd_passing
./build/dev/bin/lab_fd_passing
```

或运行 `ctest --preset dev -R '^lab_fd_passing$'`。

## 预期输出

```text
SCM_RIGHTS payload=descriptor-passing-ok verification=ok
```

## 注意与思考

控制消息应伴随至少一个普通数据字节；缓冲区大小应使用 `CMSG_SPACE`，有效消息长度使用 `CMSG_LEN`。接收方获得的是新描述符，并负责在正常及校验失败路径关闭它；还应使用 `MSG_CMSG_CLOEXEC` 或 `fcntl(FD_CLOEXEC)` 防止后续 `exec` 泄漏。`SOCK_DGRAM` 没有可靠的 EOF 通知，因此父进程若在发送描述符前失败，会显式终止并回收正在等待的子进程。实际服务还应拒绝截断或数量异常的控制消息，并验证对端身份及消息边界。思考：传递普通文件描述符后，两端的文件偏移和文件状态标志为何会相互影响？
