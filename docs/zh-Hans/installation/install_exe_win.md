# Windows 预编译 exe 安装 {#install_exe_win}

> 下载地址： mynteye-d-1.6.0-win-x64-opencv-3.4.3.exe [Google Drive](https://drive.google.com/open?id=1IP2kcnpOIWg5wQFhuMIdcToVlWUf0WD2), [百度网盘](https://pan.baidu.com/s/1zKVF4e85zrAq5-cClXnIKQ)

安装完 SDK 的 exe 安装包后，桌面会生成 SDK 根目录的快捷方式。

进入 "<SDK_ROOT_DIR>\bin\samples" 目录，双击 "get_image.exe" 运行，即可看到相机画面。

## 生成样例工程

首先，安装好 Visual Studio 2017 <https://visualstudio.microsoft.com/> 和 CMake <https://cmake.org/> 。

接着，进入 "<SDK_ROOT_DIR>\samples" 目录， 双击 "generate.bat" 即可生成样例工程 `_build\mynteye_samples.sln` 。

## 如何于 Visual Studio 2017 下使用 SDK

进入 "<SDK_ROOT_DIR>\projects\vs2017" ，见 "README.md" 说明。
