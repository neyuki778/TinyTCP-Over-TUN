# 1. 基础镜像：使用你指定的 Ubuntu 20.04 (Focal Fossa)
FROM ubuntu:24.04

# 2. 设置环境变量，避免 apt 安装时出现交互式弹窗
ENV DEBIAN_FRONTEND=noninteractive

# 3. 安装核心依赖
RUN apt update && apt install git cmake gdb build-essential clang \
clang-tidy clang-format gcc-doc pkg-config glibc-doc tcpdump tshark -y

# 4. [推荐] 创建一个非 root 用户，避免权限问题
RUN useradd -ms /bin/bash Link
USER Link

# 5. 设置工作目录
WORKDIR /home/Link/project