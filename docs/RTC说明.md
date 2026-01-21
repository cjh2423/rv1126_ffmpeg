# RTC 说明

## RTC 是什么
RTC（Real Time Clock，实时时钟）是板载的独立时钟模块，能在掉电后依靠电池/电容继续走时。

它通常通过 `/dev/rtc0` 暴露给系统，并由内核 RTC 驱动维护。

## RTC 与系统时间的关系
- **RTC 时间**：硬件时钟，开机前/断电期间仍可保持
- **系统时间**：Linux 内核维护的软件时钟，开机后由 RTC 或网络校时

常见流程：
1. 开机时由 RTC 同步系统时间（`hwclock -s`）
2. 系统运行中可被 NTP 校时
3. 关机前可将系统时间写回 RTC（`hwclock -w`）

## 典型工作方式
- 无网络环境：依赖 RTC
- 有网络环境：优先 NTP，再写回 RTC

## 常用命令
查看 RTC：
```bash
hwclock -r
```

RTC -> 系统时间：
```bash
hwclock -s
```

系统时间 -> RTC：
```bash
hwclock -w
```

手动设置系统时间：
```bash
date -s "2025-02-11 12:34:56"
```
把 RTC 写回 1970-01-01：
```bash
date -s "1970-01-01 00:00:00"
hwclock -w

验证：

hwclock -r
```

## 程序中的使用
方案 A（使用系统时间）：
- 调用 `clock_gettime(CLOCK_REALTIME, ...)` 或 `gettimeofday` 获取系统时间
- 前提是系统时间已由 RTC/NTP 校准

方案 B（直接访问 RTC）：
- 使用 `/dev/rtc0` 的 ioctl（`RTC_RD_TIME` / `RTC_SET_TIME`）
- 适合需要直接读取/写入硬件时钟的场景

## RTC 是否断电后继续计时\nRTC 在有后备电源（电池/电容）供电时会继续计时；\n若后备电源缺失或电压过低，RTC 会停走或时间漂移。\n\nRTC 本身不会“自动联网校时”，它只负责持续计时。\n系统上电后需要通过 `hwclock -s` 或 NTP 将系统时间校准，\n再用 `hwclock -w` 把正确时间写回 RTC。
注意：若 RTC 供电电压过低，会提示时间不可靠，需要先手动校时并写回 RTC。
