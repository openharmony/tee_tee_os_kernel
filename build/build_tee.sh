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
CONFIG_MK=${CHCORE_DIR}/config.mk
CHCORE_PLAT=$(sed -n 's/^[[:space:]]*CHCORE_PLAT[[:space:]]*=[[:space:]]*\([^[:space:]\\]*\).*/\1/p' ${CONFIG_MK} | head -n 1)
if [[ -z "${CHCORE_PLAT}" ]]; then
    echo "failed to get CHCORE_PLAT from ${CONFIG_MK}"
    exit 1
fi
CHCORE_LLM=$(sed -n 's/^[[:space:]]*CHCORE_LLM[[:space:]]*=[[:space:]]*\([^[:space:]\\]*\).*/\1/p' ${CONFIG_MK} | head -n 1)
if [[ "${CHCORE_LLM}" = "ON" && "${CHCORE_PLAT}" = "rk3588" ]]; then
    CHCORE_LLM=1
else
    if [[ "${CHCORE_LLM}" = "ON" ]]; then
        echo "CHCORE_LLM is only supported on rk3588; disabled for ${CHCORE_PLAT}"
    fi
    CHCORE_LLM=0
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
./build_framework.sh oh_64 ${CHCORE_DIR}/oh_tee ${COMPILER_DIR} ${COMPILER_VER} ${OH_TEE_FRAMEWORK_DIR} ${THIRD_PARTY} ${CHCORE_PLAT}
# compile again to put the apps into ramdisk-dir
cd ${CHCORE_DIR}
if [ "${CHCORE_LLM}" = 1 ]; then
cd ${CHCORE_DIR}/..
rm -rf ${CHCORE_DIR}/../oh-llama.cpp
git clone https://gitcode.com/openharmony-robot/oh-llama.cpp.git oh-llama.cpp
cd oh-llama.cpp
mkdir build
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=${OH_TOP_DIR}/prebuilts/ohos-sdk/linux/15/native/build/cmake/ohos.toolchain.cmake \
  -DLLAMA_TEE_CHCORE_LIB=${CHCORE_DIR}/libc_shared.so \
  -DGGML_NATIVE=OFF \
  -DGGML_OPENMP=OFF \
  -DLLAMA_BUILD_TESTS=OFF \
  -DLLAMA_CURL=OFF \
  -DGGML_TEE=ON
cmake --build build -j$(nproc) --target infer
cd ${CHCORE_DIR}
fi
make clean
mkdir -p ramdisk-dir
cp oh_tee/apps/* ramdisk-dir
if [ "${CHCORE_LLM}" = 1 ]; then
cp ${CHCORE_DIR}/../oh-llama.cpp/build/bin/libinfer.so ramdisk-dir
cp ${CHCORE_DIR}/../oh-llama.cpp/build/bin/libggml.so ramdisk-dir
cp ${CHCORE_DIR}/../oh-llama.cpp/build/bin/libggml-base.so ramdisk-dir
cp ${CHCORE_DIR}/../oh-llama.cpp/build/bin/libggml-cpu.so ramdisk-dir
cp ${CHCORE_DIR}/../oh-llama.cpp/build/bin/libllama.so ramdisk-dir
cp ${OH_TOP_DIR}/prebuilts/ohos-sdk/linux/15/native/llvm/lib/aarch64-linux-ohos/libc++_shared.so ramdisk-dir
rm -rf ${CHCORE_DIR}/../oh-llama.cpp
fi
make -j$(nproc) OH_DIR=${OH_TOP_DIR}
