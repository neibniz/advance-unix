# 实验 04：Linux 开放文件描述 OFD 锁

## 目标

观察 OFD 记录锁属于开放文件描述而不是进程：`dup` 得到的描述符共享锁，而再次 `open` 得到的独立开放文件描述会与它冲突。

## 关键 API

- `fcntl(..., F_OFD_SETLK, ...)`：非阻塞地设置或解除 OFD 字节范围锁。
- `struct flock`：描述锁类型、起点和范围。
- `dup`：创建引用同一开放文件描述的新文件描述符。

## 原理

程序锁住临时文件的第一个字节。通过独立 `open` 得到的竞争者应收到 `EAGAIN` 或 `EACCES`；关闭原始描述符后，因为 `dup` 描述符仍引用同一个开放文件描述，锁依旧存在。经由该副本解锁后，竞争者才能成功加锁。

## 构建与运行

本实验只在 Linux 上生成目标：

```sh
cmake --preset dev
cmake --build --preset dev --target lab_file_locking
./build/dev/bin/lab_file_locking
```

或运行 `ctest --preset dev -R '^lab_file_locking$'`。

## 预期输出

```text
OFD lock verified: duplicate kept lock, contender acquired after unlock
```

## 注意与思考

OFD 锁是 Linux 专有接口，需要内核支持；它仍属于协作锁，不会自动阻止忽略锁协议的进程读写。`l_pid` 对 OFD 锁必须为零。思考：传统 `F_SETLK` 进程锁在关闭任意指向同一文件的描述符时可能产生什么意外？
