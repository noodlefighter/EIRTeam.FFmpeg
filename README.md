![](img/cover.jpg)
# EIRTeam.FFmpeg

Note: The version of FFmpeg shipped with EIRTeam.FFmpeg allows loading of videos using the patent-encumbered h264 codec, check with your local laws to see if software patents are recognized in your country.

GDExtension Video Decoder library for [Godot Engine](https://godotengine.org) >4.1,
using the [FFmpeg](https://ffmpeg.org) library.

# Building

Currently building ffmpeg from source is supported on Linux and macOS hosts. And ffmpeg libraries for Android can be cross compiled on both hosts.
Follow these steps:
* run `git submodule update --init` to fetch ffmpeg-kit sources.
* run `make bootstrap` to install building tools.
* run `make gdextension PLATFORM=linux` for Linux.
* run `make gdextension PLATFORM=macos` for macOS.
* run `make gdextension PLATFORM=android` for Android with default arch `arm64-v8a`.
* run `make gdextension PLATFORM=android TARGET_ARCH=arm-v7a` for Android with arch `arm-v7a`.

# Documentation

The official documentation can be found [here](https://eirteam-docs.readthedocs.io/en/latest/documentation/ffmpeg/ffmpeg_getting_started.html).

# Supporting development

You can also support EIRTeam by donating on [Patreon] or purchasing [Project Heartbeat](https://store.steampowered.com/app/1216230/Project_Heartbeat/).

<a href="https://www.patreon.com/EIRTeam" target="_blank">
  <img alt="Patreon Link" src="img/patreon_support.png">
</a>

[Patreon]: https://www.patreon.com/EIRTeam


# Linux下交叉编译到Windows的笔记

安装交叉编译工具链：

```
$ sudo apt install mingw-w64
```

使用posix，修改gdextension_build/godot-cpp/tools/windows.py：

```
        env["CXX"] = prefix + "-w64-mingw32-g++-posix"
        env["CC"] = prefix + "-w64-mingw32-gcc-posix"
        env["AR"] = prefix + "-w64-mingw32-ar"
        env["RANLIB"] = prefix + "-w64-mingw32-ranlib"
        env["LINK"] = prefix + "-w64-mingw32-g++-posix"
```

下载预编译好的ffmpeg：

```
$ wget https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n6.1-latest-win64-lgpl-shared-6.1.zip
$ unzip ffmpeg-n6.1-latest-win64-lgpl-shared-6.1.zip
```

然后就可以交叉编译了：

```
cd gdextension_build
$ scons platform=windows ffmpeg_path=../ffmpeg-n6.1-latest-win64-lgpl-shared-6.1
```

编译好的插件在`gdextension_build/build/`里，拷出来就能用。
