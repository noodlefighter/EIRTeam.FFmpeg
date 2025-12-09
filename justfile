NAS_TMP_DIR:="r@r-lh.v:/home/r/proj/uav/uav-app/addons/ffmpeg/"

default:
    @just --list

build-win:
    #!/bin/bash

    ffmpeg_name="ffmpeg-N-122015-g6a14a93af5-win64-lgpl-shared"
    ffmpeg_url="https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2025-12-07-12-56/ffmpeg-N-122015-g6a14a93af5-win64-lgpl-shared.zip"
    ffmpeg_zipfile="ffmpeg-win64.zip"
    ffmpeg_bin_dir="thirdparty/ffmpeg/windows/x86_64"

    # 1. 检测x86_64-w64-mingw32-gcc-posix命令是否存在
    if ! command -v x86_64-w64-mingw32-gcc-posix &> /dev/null; then
        echo "警告: x86_64-w64-mingw32-gcc-posix 命令未找到"
        echo "尝试回退到 x86_64-w64-mingw32-gcc..."

        if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
            echo "错误: x86_64-w64-mingw32-gcc 命令也未找到"
            echo "请安装 mingw-w64 包："
            echo "  Ubuntu/Debian: sudo apt install mingw-w64"
            echo "  CentOS/RHEL: sudo yum install mingw64-gcc"
            echo "  Arch Linux: sudo pacman -S mingw-w64-gcc"
            exit 1
        fi

        echo "✓ 找到 x86_64-w64-mingw32-gcc 编译器，将使用此编译器"
        export GCC_COMPILER="x86_64-w64-mingw32-gcc"
    else
        echo "✓ 找到 x86_64-w64-mingw32-gcc-posix 编译器"
        export GCC_COMPILER="x86_64-w64-mingw32-gcc-posix"
    fi

    # 2. 修改gdextension_build/godot-cpp/tools/windows.py中的GCC编译器设置
    windows_py_file="gdextension_build/godot-cpp/tools/windows.py"
    if [ -f "${windows_py_file}" ]; then
        echo "正在修改 ${windows_py_file} 中的GCC编译器设置..."

        # 根据检测到的编译器类型进行相应的修改
        if [ "${GCC_COMPILER}" = "x86_64-w64-mingw32-gcc-posix" ]; then
            # 修改为使用 -posix 后缀的编译器
            sed -i 's|env\["CXX"\] = prefix + "-w64-mingw32-g++"|env["CXX"] = prefix + "-w64-mingw32-g++-posix"|g' "${windows_py_file}"
            sed -i 's|env\["CC"\] = prefix + "-w64-mingw32-gcc"|env["CC"] = prefix + "-w64-mingw32-gcc-posix"|g' "${windows_py_file}"
            sed -i 's|env\["LINK"\] = prefix + "-w64-mingw32-g++"|env["LINK"] = prefix + "-w64-mingw32-g++-posix"|g' "${windows_py_file}"
            echo "✓ 已设置为使用 posix 后缀的编译器"
        else
            # 修改为使用标准编译器（不包含 -posix 后缀）
            sed -i 's|env\["CXX"\] = prefix + "-w64-mingw32-g++-posix"|env["CXX"] = prefix + "-w64-mingw32-g++"|g' "${windows_py_file}"
            sed -i 's|env\["CC"\] = prefix + "-w64-mingw32-gcc-posix"|env["CC"] = prefix + "-w64-mingw32-gcc"|g' "${windows_py_file}"
            sed -i 's|env\["LINK"\] = prefix + "-w64-mingw32-g++-posix"|env["LINK"] = prefix + "-w64-mingw32-g++"|g' "${windows_py_file}"
            echo "✓ 已设置为使用标准编译器"
        fi
    else
        echo "警告: ${windows_py_file} 文件未找到"
    fi

    if [ ! -d "${ffmpeg_bin_dir}" ]; then
        echo "FFmpeg Windows构建包未找到，开始下载..."

        # 检查zip文件是否存在
        if [ ! -f ${ffmpeg_zipfile} ]; then
            echo "正在下载 FFmpeg Windows构建包..."
            wget "${ffmpeg_url}" -O "${ffmpeg_zipfile}"
            if [ $? -ne 0 ]; then
                echo "错误: FFmpeg下载失败"
                exit 1
            fi
        fi

        echo "正在解压 FFmpeg..."
        unzip "${ffmpeg_zipfile}"
        if [ $? -ne 0 ]; then
            echo "错误: FFmpeg解压失败"
            exit 1
        fi
        mkdir -p "${ffmpeg_bin_dir}"
        cp -r ${ffmpeg_name}/* ${ffmpeg_bin_dir}

        echo "✓ FFmpeg Windows构建包准备完成"
    else
        echo "✓ FFmpeg Windows构建包已存在"
    fi

    # 执行构建
    echo "开始构建 Windows版本..."
    ./build.sh \
        all \
        windows \
        4.4.0 \
        thirdparty/ffmpeg/windows/x86_64 \
        https://foo \
        foo.zip \
        true
    # cd gdextension_build
    # scons platform=windows ffmpeg_path="${ffmpeg_bin_dir}"

build-linux:
    #!/bin/bash
    set -e

    ffmpeg_name="ffmpeg-N-122015-g6a14a93af5-linux64-lgpl-shared"
    ffmpeg_url="https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2025-12-07-12-56/ffmpeg-N-122015-g6a14a93af5-linux64-lgpl-shared.tar.xz"
    ffmpeg_tarball="ffmpeg-linux.tar.xz"
    ffmpeg_bin_dir="thirdparty/ffmpeg/linux/x86_64"

    if [ ! -d "${ffmpeg_bin_dir}" ]; then
        echo "FFmpeg Linux构建包未找到，开始下载..."

        # 检查压缩包是否存在
        if [ ! -f ${ffmpeg_tarball} ]; then
            echo "正在下载 FFmpeg Linux构建包..."
            wget "${ffmpeg_url}" -O "${ffmpeg_tarball}"
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
