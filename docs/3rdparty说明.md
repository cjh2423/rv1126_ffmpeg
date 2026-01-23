# 🔧 3rdparty 依赖说明

本项目依赖于 Rockchip SDK 提供的媒体处理库及开源工具库，所有必要文件均存放在 `3rdparty/media/` 目录下。

---

## 📂 目录结构

```text
3rdparty/
└── media/
    ├── include/    # 开发头文件 (rkaiq, rk_mpi, mpp, rtsp, iniparser 等)
    ├── lib/        # 交叉编译生成的动态/静态库 (.so / .a)
    └── pkgconfig/  # 可选的 pkg-config 链路配置
```

---

## 📚 关键库说明

| 库名称 | 功能描述 | 备注 |
| :--- | :--- | :--- |
| **librockit.so** | Rockit 多媒体框架核心库 | 必选 (VI/SYS) |
| **librkaiq.so** | ISP / AIQ (图像质量转换) 相关算法库 | 必选 (ISP) |
| **librockchip_mpp.so** | Rockchip 媒体处理平台 (编解码核心) | 必选 (VENC) |
| **librtsp.a** | RTSP 协议栈静态库 | 必选 (网络推流) |
| **librksysutils.so** | Rockchip 系统工具封装库 | 必选 |
| **librkmuxer.so** | Rockchip 封装复用库 (RTMP 推流核心) | 必选 (RTMP) |
| **librga.so** | Rockchip 2D 图形加速库 | 必选 (RGA) |
| **libiniparser.a** | INI 配置文件解析库 | 内部集成 |

---

## ⚙️ 编译与运行配置

### 1. 编译时链接
`CMakeLists.txt` 中通过 `MEDIA_DIR` 宏定义自动寻找并链接该目录下的库文件。

### 2. 运行时加载
程序运行过程中需要从系统的动态库搜索路径加载 `.so` 文件。
- **推荐做法**: 在 `run.sh` 中设置 `LD_LIBRARY_PATH` 指向板端存放这些库的路径。
- **自启动方案**: 修改 `/etc/ld.so.conf` 并执行 `ldconfig`。

---

## ⚠️ 更新说明
如需更新 SDK 版本：
1. 从 SDK `out/media` 目录拷贝最新库文件。
2. 确保 `include` 下的各模块头文件同步更新。
3. 重新编译项目并替换板端二进制文件。
