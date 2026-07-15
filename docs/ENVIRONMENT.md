# 构建与远端环境

## 已验证环境

项目以以下环境作为可重复构建基线：

| 项目 | 值 |
|---|---|
| SSH | `root@192.168.31.192 -i ~/.ssh/ssh_linkwater` |
| 允许使用的宿主目录 | `/data` |
| 宿主架构 | `linux/amd64` |
| 容器镜像 | `ghcr.io/neibniz/clang-dev:7ee244e55f19` |
| 镜像 ID | `sha256:2e9b7a5e0767e2c2e49251bb27f77b4dc8814b3f0075661a7bae992baa35a9c9` |
| Clang | 22.1.8 |
| CMake | 4.3.4 |
| Ninja | 1.12.1 |

镜像已经存在于远端，因此构建脚本使用 `--pull=never`，不会隐式改变镜像版本。

`dev` 和 `sanitizers` build preset 都启用 `cleanFirst`。本机与远端的时钟可能不同，而 `rsync -a` 会保留本机源码时间；全量重编译可避免 Ninja 因对象文件时间较新而误用旧二进制。实验规模很小，因此这项确定性检查的成本很低。

## 目录边界

远端源码和构建产物都位于：

```text
/data/advance-unix/
├── build/       # CMake 生成物
├── experiments/ # 源码
└── ...
```

`scripts/container-build.sh` 会解析项目的真实路径；只要它不等于 `/data` 或不在 `/data/` 下，脚本就立即退出。Docker 只把这一项目目录挂载到容器的 `/workspace`，没有挂载宿主其他目录。

## 手工运行容器

需要进入容器观察单个 API 时，在远端执行：

```sh
cd /data/advance-unix
docker run --rm -it \
  --pull=never \
  --network=none \
  --mount type=bind,src=/data/advance-unix,dst=/workspace \
  --workdir /workspace \
  ghcr.io/neibniz/clang-dev:7ee244e55f19 \
  bash
```

容器内再运行：

```sh
cmake --workflow --preset verify
```

## 只清理构建产物

不要删除整个 `/data` 或使用不受约束的通配符。需要全量重编译时只删除项目自己的构建目录：

```sh
rm -rf /data/advance-unix/build
```
