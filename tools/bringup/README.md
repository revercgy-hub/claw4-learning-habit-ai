# Bring-up 辅助工具

`host-preflight.ps1` 只检查开发主机和官方源码；它不写设备、不刷机、不安装驱动，也不会修改官方仓库。

在项目根目录运行：

```powershell
.\tools\bringup\host-preflight.ps1
```

在设备连接前，预期结果是缺少或尚未导出 ESP-IDF、且没有串口。安装并导出 ESP-IDF v5.5.4 后再次运行。设备接入后，脚本会列出 COM 端口；仍须在 Windows 设备管理器中按描述符确认 P4 端口。
