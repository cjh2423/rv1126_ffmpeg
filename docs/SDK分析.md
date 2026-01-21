# SDK 分析

当前工程基于 RV1126 IPC SDK 的媒体能力，主要使用：
- ISP/VI/VENC 的 RK MPI 接口
- Rockit 与 MPP 库

工具链使用 SDK 内置路径：
`/home/u22/sdk/rv1126_ipc_linux_release/tools/linux/toolchain/arm-rockchip830-linux-gnueabihf`

头文件与库路径：
- 头文件：`3rdparty/media/include`
- 库：`3rdparty/media/lib`

如果 SDK 更新，请同步复制新的 `media` 目录并更新 CMake。
