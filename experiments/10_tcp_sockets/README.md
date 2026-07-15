# 实验 10：非阻塞 TCP 连接与 loopback 自验证

## 目标

在单个进程中建立仅使用 IPv4 loopback 的 TCP 客户端和服务端，练习地址解析、非阻塞连接完成检查以及连接接收。

## 关键 API

- `getaddrinfo`：把数字主机和服务转换为套接字地址。
- `socket`、`bind`、`listen`、`getsockname`：监听内核分配的临时端口。
- `fcntl`、`connect`、`poll`、`getsockopt(SO_ERROR)`：发起并确认非阻塞连接。
- `accept4`：接收连接并原子设置 `SOCK_CLOEXEC`。
- `send`、`recv`：完成 `ping`/`pong` 双向交换。

## 原理

服务端通过 `getaddrinfo` 得到 `127.0.0.1:0`，绑定后用 `getsockname` 取得实际端口。客户端再次解析该端口，把套接字设为非阻塞并调用 `connect`；若连接仍在进行，就等待 `POLLOUT`，随后读取 `SO_ERROR` 判断真实结果。连接成功后恢复阻塞模式，服务端用 `accept4` 接收连接，双方交换并校验固定消息。

## 构建与运行

本实验因使用 `accept4` 标记为 Linux-only，且完全不访问外网：

```sh
cmake --preset dev
cmake --build --preset dev --target lab_tcp_sockets
./build/dev/bin/lab_tcp_sockets
```

或运行 `ctest --preset dev -R '^lab_tcp_sockets$'`。

## 预期输出

端口由内核动态分配：

```text
loopback=127.0.0.1:43210 request=ping response=pong verification=ok
```

## 注意与思考

`poll` 报告可写只说明连接流程结束，不保证成功，必须再读取 `SO_ERROR`。流式套接字没有消息边界，生产代码必须处理短发送、短接收、关闭和超时；本实验用循环传输固定长度。思考：若同时支持 IPv4 和 IPv6，应如何遍历 `getaddrinfo` 返回的地址链并处理逐个失败？
