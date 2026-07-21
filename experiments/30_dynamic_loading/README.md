# 实验 30：运行时动态加载

## 目标

运行时打开 glibc 环境的数学库，解析 `cos` 函数，按目标 ABI 构造函数指针并验证 `cos(0.0) == 1.0`，最后关闭共享对象。

## 书籍主题

- APUE：程序装载、共享库、进程地址空间与运行时环境。
- TLPI：共享库、动态链接器、运行时符号查找。

## 关键 API

- `dlopen(3)`：以 `RTLD_NOW | RTLD_LOCAL` 装载 `libm.so.6`。
- `dlerror(3)`：读取并清除动态链接器的线程局部错误状态。
- `dlsym(3)`：按名称查找 `cos` 符号。
- `dlclose(3)`：释放对共享对象的引用。

## 原理

调用 `dlsym` 前先调用一次 `dlerror` 清除旧错误，再在查找后读取错误状态。为避免直接强制转换触发编译器诊断，程序在固定 Linux/glibc ABI 上验证两种指针大小一致，并用 `memcpy` 复制表示；随后调用解析出的函数并自校验结果。这是目标 ABI/POSIX 实现约定，不是 ISO C 对任意平台的保证。

## 原理插图

![实验 30：运行时动态加载原理插图](https://oss.euler.icu/teaser/advance-unix/principles/30_dynamic_loading.png)

> 蓝色表示用户空间，琥珀色表示内核对象，绿色表示成功路径，珊瑚色表示语义边界或失败路径。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_dynamic_loading
./build/dev/bin/lab_dynamic_loading
```

## 预期输出

```text
dlopen(libm.so.6) resolved cos(0.0) = 1.0
```

## 平台边界

`dlopen` 系列属于 POSIX 常见扩展，但 `libm.so.6` 是 Linux/glibc 的 SONAME，musl 和其他 Unix-like 系统可能使用不同名称或将数学函数放在不同对象中。因此本实验是 Linux/glibc-only；CMake 会检测 `__GLIBC__`，不满足时跳过目标，并通过 `${CMAKE_DL_LIBS}` 适配是否仍需显式链接 `libdl`。

## 注意事项

- `dlsym` 返回空指针不一定单独代表失败，应以紧随其后的 `dlerror` 判断。
- `RTLD_LOCAL` 避免新符号默认暴露给之后加载的共享对象。
- `dlclose` 减少引用计数，但不保证代码立即从地址空间卸载。

## 思考

插件 ABI 如果跨编译器版本共享结构体和函数指针，需要额外约定哪些版本、大小和所有权信息？
