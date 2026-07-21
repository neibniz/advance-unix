# 实验 12：用 inotify 观察文件系统事件

## 目标

监控一个临时目录，并由程序自行创建、写入和删除文件，验证完整的 inotify 事件读取流程。

## 关键 API

- `inotify_init1`：创建非阻塞且 close-on-exec 的通知描述符。
- `inotify_add_watch`、`inotify_rm_watch`：添加和移除目录监控。
- `poll`、`read`：等待并批量读取可变长 `inotify_event` 记录。
- `mkdtemp`、`open`、`write`、`unlink`、`rmdir`：生成测试事件并清理临时资源。

## 原理

程序在运行时当前目录建立唯一临时目录，注册 `IN_CREATE`、`IN_CLOSE_WRITE`、`IN_DELETE` 等事件，然后创建 `sample.txt`、写入内容并删除它。事件可能一次读取多条，因此程序按每条记录的固定头和 `len` 字段步进，直到观察到三个必需事件。最后移除 watch 并删除目录；按项目约定运行时，该临时资源始终位于 `/data` 下的项目或构建目录中。

## 原理插图

![实验 12：用 inotify 观察文件系统事件原理插图](https://oss.euler.icu/teaser/advance-unix/principles/12_filesystem_events.png)

> 蓝色表示用户空间，琥珀色表示内核对象，绿色表示成功路径，珊瑚色表示语义边界或失败路径。

## 构建与运行

本实验只在 Linux 上生成：

```sh
cmake --preset dev
cmake --build --preset dev --target lab_filesystem_events
./build/dev/bin/lab_filesystem_events
```

或运行 `ctest --preset dev -R '^lab_filesystem_events$'`。

## 预期输出

```text
file=sample.txt events=create,close_write,delete verification=ok
```

## 注意与思考

inotify 提供的是事件流而不是文件系统事务日志：相同事件可能合并，队列可能溢出，监控目录也不会自动递归覆盖新子目录。本实验把 `IN_Q_OVERFLOW` 视为自验证失败；生产程序通常需要重新扫描来重建状态。事件发生后文件状态还可能再次改变，因此收到通知后通常要重新查询实际状态。思考：需要可靠维护大型目录树索引时，应如何处理监控目录被移动的情况？
