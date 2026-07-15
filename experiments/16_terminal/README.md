# 实验 16：伪终端与 termios

## 目标

不依赖交互式终端，创建一对 PTY 主从设备，配置从端的终端属性，并验证双向数据传输。

## 关键 API

- `posix_openpt(3)`、`grantpt(3)`、`unlockpt(3)`、`ptsname(3)`：建立 PTY 主从端。
- `tcgetattr(3)`、`tcsetattr(3)`：读取和修改终端行规程。
- `termios` 标志：关闭规范模式、回显、信号字符处理和输出转换。

## 原理

PTY 主端模拟终端驱动一侧，从端表现得像普通终端设备。程序把从端切换到 raw 风格模式，分别执行“主端写、从端读”和“从端写、主端读”，再比较原始字节。退出前恢复最初的 `termios`。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_terminal
./build/dev/bin/lab_terminal
```

## 预期输出

```text
PTY /dev/pts/3 transferred data in both directions
```

设备编号每次运行可能不同。

## 注意事项

- 这些接口属于 POSIX/XSI；容器需要可用的 `devpts` 挂载。
- 规范模式会按“行”交付输入，回显还会把输入送回主端；实验主动关闭它们。
- 生产程序修改真实终端时，还要处理信号和异常退出，以确保属性得到恢复。

## 思考

终端模拟器、SSH 服务端和测试框架为什么需要 PTY，而不是两条普通管道？
