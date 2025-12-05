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


# Linux下编译笔记

将编译到Win和Linux，需要安装交叉编译工具链：

```
$ sudo apt install mingw-w64
```

然后就可以用just命令编译了：

```
$ just build-linux
$ just build-win
```

编译好的插件在`gdextension_build/build/`里，拷出来就能用。
