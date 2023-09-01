# Copyright (c) 2023 Institute of Parallel And Distributed Systems (IPADS), Shanghai Jiao Tong University (SJTU)
# Licensed under the Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#     http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
# PURPOSE.
# See the Mulan PSL v2 for more details.
set -e

# paths setting
OH_TOP_DIR=$1
CHCORE_DIR=$2
OH_TEE_FRAMEWORK_DIR=$3
COMPILER_DIR=$4
COMPILER_VER=$5
if [[ -z "$OH_TOP_DIR" ]]; then
    OH_TOP_DIR=$(pwd)/../../../..
fi
if [[ -z "$CHCORE_DIR" ]]; then
    CHCORE_DIR=$(pwd)/..
fi
if [[ -z "$OH_TEE_FRAMEWORK_DIR" ]]; then
    OH_TEE_FRAMEWORK_DIR=$(pwd)/../../tee_os_framework
fi
OH_TEE_PREBUILD_DIR=${OH_TEE_FRAMEWORK_DIR}/prebuild
OH_TEE_HEADERS_DIR=${OH_TEE_PREBUILD_DIR}/tee-kernel-local-release/headers
OH_TEE_LIBS_DIR=${OH_TEE_PREBUILD_DIR}/tee-kernel-local-release/libs/aarch64
THIRD_PARTY=${OH_TOP_DIR}/third_party
# default compiler and version setting
if [[ -z "$COMPILER_DIR" ]]; then
    COMPILER_DIR=${OH_TOP_DIR}/prebuilts/clang/ohos/linux-x86_64/llvm
fi
if [[ -z "$COMPILER_VER" ]]; then
    COMPILER_VER=15.0.4
fi
# clean framework
cd ${OH_TEE_FRAMEWORK_DIR}/build
./clean_framework.sh ${OH_TEE_FRAMEWORK_DIR}
# compile chcore
cd ${CHCORE_DIR}
rm -rf oh_tee
make clean && make -j$(nproc) CHCORE_COMPILER_DIR=${COMPILER_DIR} OH_DIR=${OH_TOP_DIR}

mkdir -p ramdisk-dir
cp libc_shared.so ${CHCORE_DIR}/ramdisk-dir/libc_shared.so


mkdir -p ${OH_TEE_LIBS_DIR}
cp libc_shared.so ${OH_TEE_LIBS_DIR}/libc_shared.so
mkdir -p ${OH_TEE_FRAMEWORK_DIR}/output/aarch64/libs/
cp libc_shared.so ${OH_TEE_FRAMEWORK_DIR}/output/aarch64/libs/libc_shared.so
mkdir -p ${OH_TEE_FRAMEWORK_DIR}/output/aarch64
cp libc_shared.so ${OH_TEE_FRAMEWORK_DIR}/output/aarch64/libc_shared.so

# libc and libohtee headers for framework
mkdir -p ${OH_TEE_HEADERS_DIR}/libc
mkdir -p ${OH_TEE_HEADERS_DIR}/sys
cp -r ${CHCORE_DIR}/user/chcore-libc/musl-libc/install/include/* ${OH_TEE_HEADERS_DIR}/libc/
cp ${CHCORE_DIR}/user/chcore-libs/sys-libs/libohtee/include/* ${OH_TEE_HEADERS_DIR}/sys/
# go to framework and build it
cd ${OH_TEE_FRAMEWORK_DIR}/build
./build_framework.sh oh_64 ${CHCORE_DIR}/oh_tee ${COMPILER_DIR} ${COMPILER_VER} ${OH_TEE_FRAMEWORK_DIR} ${THIRD_PARTY}
# compile again to put the apps into ramdisk-dir
cd ${CHCORE_DIR}
make clean
mkdir -p ramdisk-dir
cp oh_tee/apps/* ramdisk-dir
make -j$(nproc) OH_DIR=${OH_TOP_DIR}
