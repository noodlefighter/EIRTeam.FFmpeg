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


build-android:
    #!/bin/bash
    set -e

    # 1. 检查Java环境
    java_dir="/usr/lib/jvm/java-25-openjdk"
    if [ -d "${java_dir}" ]; then
        echo "✓ 找到 Java 25 OpenJDK"
        export JAVA_HOME="${java_dir}"
    else
        echo "错误: 未找到 /usr/lib/jvm/java-25-openjdk 目录"
        echo "请手动修改 justfile 中的 JAVA_HOME 路径，要求 JAVA 11 以上版本"
        echo "或者安装 OpenJDK: sudo apt install openjdk-11-jdk 或更高版本"
        exit 1
    fi

    # 2. 检查并安装Android SDK
    sdk_path="$PWD/android-sdk"
    if [ ! -d "${sdk_path}" ]; then
        echo "Android SDK未找到，开始下载..."

        sdk_url="https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip"
        sdk_zipfile="android-cmdline-tools.zip"

        # 检查本地缓存
        if [ ! -f "${sdk_zipfile}" ]; then
            echo "正在下载 Android SDK Command Line Tools..."
            wget "${sdk_url}" -O "${sdk_zipfile}"
            if [ $? -ne 0 ]; then
                echo "错误: Android SDK下载失败"
                exit 1
            fi
        else
            echo "✓ 使用缓存的 Android SDK 压缩包"
        fi

        echo "正在解压 Android SDK..."
        mkdir -p android-sdk/cmdline-tools
        unzip -q "${sdk_zipfile}" -d android-sdk/
        echo "✓ Android SDK 解压完成"

    else
        echo "✓ Android SDK 已存在"
    fi

    echo "正在安装 Android SDK 组件..."
    yes | android-sdk/cmdline-tools/bin/sdkmanager --sdk_root="${sdk_path}" "platform-tools" "build-tools;30.0.3" "platforms;android-29" "cmdline-tools;latest" "cmake;3.18.1"
    if [ $? -ne 0 ]; then
        echo "错误: Android SDK 组件安装失败"
        exit 1
    fi
    echo "✓ Android SDK 组件安装完成"

    # 3. 检查并安装Android NDK
    ndk_path="${sdk_path}/ndk/23.2.8568313"
    if [ ! -d "${ndk_path}" ]; then
        echo "Android NDK未找到，开始下载..."

        ndk_url="https://dl.google.com/android/repository/android-ndk-r23c-linux.zip"
        ndk_zipfile="android-ndk-r23c-linux.zip"
        ndk_unzip_folder="android-ndk-r23c"

        # NDK下载页：https://github.com/android/ndk/wiki/Unsupported-Downloads

        # 检查本地缓存
        if [ ! -f "${ndk_zipfile}" ]; then
            echo "正在下载 Android NDK..."
            wget "${ndk_url}" -O "${ndk_zipfile}"
            if [ $? -ne 0 ]; then
                echo "错误: Android NDK下载失败"
                exit 1
            fi
        else
            echo "✓ 使用缓存的 Android NDK 压缩包"
        fi

        echo "正在解压 Android NDK..."
        unzip -o -q "${ndk_zipfile}"
        if [ $? -ne 0 ]; then
            echo "错误: Android NDK解压失败"
            exit 1
        fi
        echo "✓ Android NDK 解压完成"
        mkdir -p "$PWD/android-sdk/ndk/"
        mv "${ndk_unzip_folder}" "${ndk_path}"
    else
        echo "✓ Android NDK 已存在"
    fi

    # 4. 设置环境变量
    export ANDROID_SDK_ROOT="$PWD/android-sdk"
    export ANDROID_NDK_HOME="${ndk_path}"
    export PATH="${ANDROID_SDK_ROOT}/cmake/3.18.1/bin/:$PATH"

    echo "✓ 环境变量设置完成:"
    echo "  JAVA_HOME=${JAVA_HOME}"
    echo "  ANDROID_SDK_ROOT=${ANDROID_SDK_ROOT}"
    echo "  ANDROID_NDK_HOME=${ANDROID_NDK_HOME}"
    echo "  PATH=${PATH}"

    # 构建ffmpeg
    PLATFORM=android TARGET_ARCH=arm64-v8a make ffmpeg

    # 5. 执行godot插件构建
    echo "开始构建 Android版本..."
    ./build.sh \
        all \
        android \
        4.4.0 \
        thirdparty/ffmpeg/android/arm64 \
        https://foo \
        foo.tar.xz \
        true

sync:
    rsync -r gdextension_build/build/addons/ffmpeg/ {{NAS_TMP_DIR}}
