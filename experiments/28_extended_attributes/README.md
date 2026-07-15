# 实验 28：文件扩展属性

## 目标

仅通过已打开的文件描述符设置、读取、列举并删除一个 `user.*` 扩展属性，最后验证已删除的属性返回 `ENODATA`。

## 书籍主题

- APUE：文件属性、文件描述符以及基于描述符操作相对路径操作的竞态优势。
- TLPI：扩展文件属性、命名空间和文件系统元数据。

## 关键 API

- `fsetxattr(2)`：设置扩展属性，本实验以 `XATTR_CREATE` 防止意外覆盖。
- `fgetxattr(2)`：先查询值长度，再读取属性值。
- `flistxattr(2)`：取得由 NUL 分隔的属性名列表。
- `fremovexattr(2)`：删除指定属性。

## 原理

程序在临时文件上创建 `user.advance_unix`，验证查询到的长度和值，再遍历 `flistxattr` 返回的变长名称列表。删除属性后再次读取必须失败，且 `errno` 必须是 `ENODATA`。所有操作结束后关闭并删除临时文件。

## 构建与运行

```sh
cmake --preset dev
cmake --build --preset dev --target lab_extended_attributes
./build/dev/bin/lab_extended_attributes
```

## 预期输出

```text
xattr user.advance_unix set, listed, read, and removed; ENODATA verified
```

## 平台边界

本实验使用 Linux 的 `<sys/xattr.h>` 和五参数 `fsetxattr`（fd、名称、值、长度、标志）接口，因此目标为 Linux-only。其他 Unix-like 系统可能提供同名 API，但参数、命名空间和错误码并不完全相同；文件系统、挂载选项或安全策略也可能禁用扩展属性。

## 注意事项

- Linux 的 `user.*` 属性通常要求调用者对文件有相应权限，且并非所有文件类型或文件系统都支持。
- `flistxattr` 返回的是紧凑的 NUL 分隔序列，不是单个普通字符串。
- 先查询长度再读取仍可能遭遇并发修改；生产代码应处理 `ERANGE` 并重试。

## 思考

在路径可能被其他进程替换时，为什么先安全地打开文件再使用 `f*xattr` 接口比路径版本更可靠？
