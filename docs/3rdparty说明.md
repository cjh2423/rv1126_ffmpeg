# 3rdparty 说明

本工程依赖 SDK 的媒体库，集中放在 `3rdparty/media/`。目录结构如下：

```
3rdparty/
  media/
    include/    # 头文件（rkaiq、rk_mpi 等）
    lib/        # 动态/静态库（rockit、rkaiq、mpp、rga、drm 等）
    pkgconfig/  # 可选的 pkg-config 描述文件
    readme.txt  # SDK 相关说明
```

- `librockit.so`：Rockit 媒体框架
- `librkaiq.so`：ISP/AIQ 相关
- `librockchip_mpp.so`：MPP 编码/解码
- `librga.so`：RGA 加速
- `librtsp.a`：RTSP（当前未启用）

构建时会链接 `3rdparty/media/lib`，运行时需确保这些库在板端可被加载。

如果库路径变化，请同步更新 `CMakeLists.txt` 的 `MEDIA_DIR`。
