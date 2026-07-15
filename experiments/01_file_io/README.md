# 实验 01：定位 I/O 与分散/聚集 I/O

## 目标

用一个小文件同时练习定位读写与分散/聚集读写，并验证定位 I/O 不会改变共享文件偏移量。

## 关键 API

- `pread`、`pwrite`：在显式偏移处读写，不修改文件描述符的当前偏移。
- `readv`、`writev`：一次系统调用在多个缓冲区之间传输数据。
- `lseek`：观察或调整当前文件偏移。

## 原理

程序先用 `writev` 写入 `hello world`，再用 `pwrite` 将后半段原地改为 `UNIX!`。随后以 `pread` 校验完整内容及当前偏移，最后用三个 `iovec` 通过 `readv` 将内容读回不同缓冲区并重新组合。

## 构建与运行

在项目根目录执行：

```sh
cmake --preset dev
cmake --build --preset dev --target lab_file_io
./build/dev/bin/lab_file_io
```

也可以用 `ctest --preset dev -R '^lab_file_io$'` 运行自验证。

## 预期输出

```text
file I/O verified: hello UNIX!
```

## 注意与思考

临时文件位于运行时当前目录，并在退出前删除。真实程序仍需处理短读、短写；本实验对 `pread` 做循环读取，而对本地普通文件上的小型 `readv`/`writev` 直接验证总字节数。思考：多线程共享同一个文件描述符时，为什么定位 I/O 通常比 `lseek` 后再读写更安全？
