# 实验 13：零拷贝传输

## 目标

使用 Linux `splice(2)` 把普通文件内容直接送入管道，并验证接收内容，理解“零拷贝”接口避免用户态中转缓冲区的意义。

## 关键 API

- `mkstemp(3)`、`unlink(2)`：创建并立即匿名化临时文件。
- `pipe2(2)`：创建带 `CLOEXEC` 标志的管道。
- `splice(2)`：在两个文件描述符之间移动页缓存数据。

## 原理

程序先把一段已知文本写入临时文件，再以显式文件偏移调用 `splice`，将数据移动到管道写端。随后从管道读端读回同样长度的数据，并逐字节比较。数据内容不会先由 `read` 复制到程序缓冲区再 `write` 到管道。

## 原理插图

![实验 13：零拷贝传输原理插图](https://oss.euler.icu/teaser/advance-unix/principles/13_zero_copy.png)

> 蓝色表示用户空间，琥珀色表示内核对象，绿色表示成功路径，珊瑚色表示语义边界或失败路径。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_zero_copy
./build/dev/bin/lab_zero_copy
```

## 预期输出

```text
splice transferred 56 bytes and content matched
```

## 注意事项

- 本实验仅适用于 Linux；`splice` 不是 POSIX API。
- 实际性能收益取决于文件系统、数据规模和后续消费者。
- 管道容量有限；真实程序必须处理短传输和背压。
- 临时文件在打开后立即 `unlink`，进程退出时不会遗留文件。

## 思考

如果目标是套接字，`sendfile(2)` 与 `splice(2)` 的约束和适用场景有何不同？
