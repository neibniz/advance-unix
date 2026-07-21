# 实验 24：线程特定数据

## 目标

让三个线程各自保存一份私有数据，验证一次性初始化、线程内取回数据，以及线程退出时自动调用析构函数。

## 书籍对应主题

- **APUE**：线程同步、线程特定数据、线程退出清理。
- **TLPI**：POSIX 线程、线程特有数据与一次性初始化。

## 关键 API

- `pthread_once(3)`：无论多少线程竞争，只初始化键一次。
- `pthread_key_create(3)`、`pthread_key_delete(3)`：创建带析构函数的进程级键。
- `pthread_setspecific(3)`、`pthread_getspecific(3)`：关联和读取当前线程的值。
- `pthread_create(3)`、`pthread_join(3)`：创建线程并等待析构完成。

## 原理

三个线程都经过同一个 `pthread_once` 控制对象创建键，然后分别分配包含线程编号的数据并写入该键。每个线程读取自己的值完成校验后直接返回；只要键中的值非空，线程库就会调用析构函数。析构函数释放内存，并用 C 原子计数器累计调用次数和编号总和。主线程 `join` 后验证析构恰好发生三次且总和为 6。

## 原理插图

![实验 24：线程特定数据原理插图](https://oss.euler.icu/teaser/advance-unix/principles/24_thread_specific_data.png)

> 蓝色表示用户空间，琥珀色表示内核对象，绿色表示成功路径，珊瑚色表示语义边界或失败路径。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_thread_specific_data
./build/dev/bin/lab_thread_specific_data
```

也可以运行 `ctest --preset dev -R '^lab_thread_specific_data$'`。

## 预期输出

```text
threads=3 destructors=3 id_sum=6 verification=ok
```

## 注意与思考

键由进程内所有线程共享，但每个线程与键关联的值不同。`pthread_key_delete` 不会替仍存活的线程调用析构函数，因此本实验仅在所有已创建线程都成功 `join` 后删除键；任一 `join` 失败时保留键并返回失败，让进程终止回收地址空间，而不冒险影响可能仍在运行的线程。思考：析构函数再次设置非空值时，POSIX 为什么允许有限次数的重复析构？
