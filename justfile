NAS_TMP_DIR:="r@r-lh.v:/home/r/proj/uav/uav-app/addons/ffmpeg/"

default:
    @just --list

build-win:
    #!/bin/bash

    # 1. 检测x86_64-w64-mingw32-gcc-posix命令是否存在
    if ! command -v x86_64-w64-mingw32-gcc-posix &> /dev/null; then
        echo "错误: x86_64-w64-mingw32-gcc-posix 命令未找到"
        echo "请安装 mingw-w64 包："
        echo "  Ubuntu/Debian: sudo apt install mingw-w64"
        echo "  CentOS/RHEL: sudo yum install mingw64-gcc"
        echo "  Arch Linux: sudo pacman -S mingw-w64-gcc"
        exit 1
    fi

    echo "✓ 找到 x86_64-w64-mingw32-gcc-posix 编译器"

    # 2. 将windows.py覆盖gdextension_build/godot-cpp/tools/windows.py
    if [ -f "windows.py" ]; then
        cp windows.py gdextension_build/godot-cpp/tools/windows.py
        echo "✓ 已更新 gdextension_build/godot-cpp/tools/windows.py"
    else
        echo "警告: 当前目录下未找到 windows.py 文件"
    fi

    # 3. 检查ffmpeg-n6.1-latest-win64-lgpl-shared-6.1文件夹是否存在
    if [ ! -d "ffmpeg-n6.1-latest-win64-lgpl-shared-6.1" ]; then
        echo "FFmpeg Windows构建包未找到，开始下载..."

        # 检查zip文件是否存在
        if [ ! -f "ffmpeg-n6.1-latest-win64-lgpl-shared-6.1.zip" ]; then
            echo "正在下载 FFmpeg Windows构建包..."
            wget https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n6.1-latest-win64-lgpl-shared-6.1.zip
            if [ $? -ne 0 ]; then
                echo "错误: FFmpeg下载失败"
                exit 1
            fi
        fi

        echo "正在解压 FFmpeg..."
        unzip ffmpeg-n6.1-latest-win64-lgpl-shared-6.1.zip
        if [ $? -ne 0 ]; then
            echo "错误: FFmpeg解压失败"
            exit 1
        fi

        echo "✓ FFmpeg Windows构建包准备完成"
    else
        echo "✓ FFmpeg Windows构建包已存在"
    fi

    # 执行构建
    echo "开始构建 Windows版本..."
    cd gdextension_build
    scons platform=windows ffmpeg_path=../ffmpeg-n6.1-latest-win64-lgpl-shared-6.1

build-linux:
    #!/bin/bash
    set -e

    ffmpeg_name="ffmpeg-N-122015-g6a14a93af5-linux64-lgpl-shared"
    ffmpeg_url="https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2025-12-07-12-56/ffmpeg-N-122015-g6a14a93af5-linux64-lgpl-shared.tar.xz"
    ffmpeg_tarball="ffmpeg-linux.tar.xz"
    ffmpeg_bin_dir="thirdparty/ffmpeg/linux/x86_64"

    if [ ! -d "thirdparty/ffmpeg/linux/x86_64" ]; then
        echo "FFmpeg Linux构建包未找到，开始下载..."

        # 检查压缩包是否存在
        if [ ! -f ${ffmpeg_tarball} ]; then
            echo "正在下载 FFmpeg Linux构建包..."
            wget https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2025-12-07-12-56/ffmpeg-N-122015-g6a14a93af5-linux64-lgpl-shared.tar.xz -O ${ffmpeg_tarball}
            if [ $? -ne 0 ]; then
                echo "错误: FFmpeg下载失败"
                exit 1
            fi
        fi

        echo "正在解压 FFmpeg..."
        tar -xvf ${ffmpeg_tarball}
        mkdir -p ${ffmpeg_bin_dir}
        cp -r ${ffmpeg_name}/* ${ffmpeg_bin_dir}

        if [ $? -ne 0 ]; then
            echo "错误: FFmpeg解压失败"
            exit 1
        fi

        echo "✓ FFmpeg Linux构建包准备完成"
    else
        echo "✓ FFmpeg Linux构建包已存在"
    fi

    ./build.sh \
        all \
        linux \
        4.4.0 \
        thirdparty/ffmpeg/linux/x86_64 \
        https://foo \
        ffmpeg-linux.tar.xz \
        true


sync:
    rsync -r gdextension_build/build/addons/ffmpeg/ {{NAS_TMP_DIR}}
