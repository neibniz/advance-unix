# 实验 07：互斥量与条件变量

## 目标

用一个有界队列实现最小生产者—消费者模型，理解互斥量保护共享状态、条件变量等待状态变化的配合方式。

## 关键 API

- `pthread_create`、`pthread_join`：创建线程并等待线程结束。
- `pthread_mutex_lock`、`pthread_mutex_unlock`：串行访问队列及统计数据。
- `pthread_cond_wait`：等待时原子地释放互斥量，返回前重新获得它。
- `pthread_cond_signal`：在入队或出队后唤醒一个可能满足条件的线程。

## 原理

队列容量为 4。生产者依次放入 1 到 20，队列满时等待 `not_full`；消费者取出 20 个值，队列空时等待 `not_empty`。条件变量只表示“状态可能改变”，真正的队列条件始终在持有互斥量时用 `while` 重新检查。主线程等待两者结束，并验证生产/消费计数、空队列以及总和 210。

## 原理插图

![实验 07：互斥量与条件变量原理插图](https://oss.euler.icu/teaser/advance-unix/principles/07_threads.png)

> 蓝色表示用户空间，琥珀色表示内核对象，绿色表示成功路径，珊瑚色表示语义边界或失败路径。

## 构建与运行

在项目根目录执行：

```sh
cmake --preset dev
cmake --build --preset dev --target lab_threads
./build/dev/bin/lab_threads
```

也可以运行 `ctest --preset dev -R '^lab_threads$'`。

## 预期输出

```text
produced=20 consumed=20 sum=210 verification=ok
```

## 注意与思考

条件变量允许虚假唤醒，因此不能用 `if` 代替谓词循环。队列状态、谓词检查和状态修改必须受同一互斥量保护。pthread API 通常直接返回错误码，而不是设置 `errno`。思考：增加多个生产者和消费者后，何时应使用 `pthread_cond_broadcast`，何时 `signal` 已足够？
