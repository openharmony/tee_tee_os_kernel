# Copyright (c) 2023 Institute of Parallel And Distributed Systems (IPADS), Shanghai Jiao Tong University (SJTU)
# Licensed under the Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#     http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
# PURPOSE.
# See the Mulan PSL v2 for more details.

CHCORE_DIR=$(pwd)
OH_TEE_FRAMEWORK_DIR=${CHCORE_DIR}/../tee_tee_os_framework/
OH_TEE_PREBUILD_DIR=${OH_TEE_FRAMEWORK_DIR}/prebuild/
OH_TEE_HEADERS_DIR=${OH_TEE_PREBUILD_DIR}/tee-kernel-local-release/headers/
OH_TEE_LIBS_DIR=${OH_TEE_PREBUILD_DIR}/tee-kernel-local-release/libs/aarch64
THIRD_PARTY=${CHCORE_DIR}/../../../third_party/


# compile chcore
make clean && make -j$(nproc)

rm -rf oh_tee
rm -rf ramdisk-dir

# build bounds_checking_function
cd ${THIRD_PARTY}/bounds_checking_function
make CC="clang -target aarch64-linux-gnu"

# link libc with bounds checking functions and libtee
# and copy to framework
cd ${CHCORE_DIR}/user/chcore-libc/musl-libc
make libc_shared
mkdir -p ${OH_TEE_LIBS_DIR}
cp libc_shared.so ${OH_TEE_LIBS_DIR}/libc_shared.so
mkdir -p ${OH_TEE_FRAMEWORK_DIR}/output/aarch64/libs/
cp libc_shared.so ${OH_TEE_FRAMEWORK_DIR}/output/aarch64/libs/libc_shared.so
mkdir -p ${OH_TEE_FRAMEWORK_DIR}/output/aarch64
cp libc_shared.so ${OH_TEE_FRAMEWORK_DIR}/output/aarch64/libc_shared.so

# libc and libohtee headers for framework
cd ${CHCORE_DIR}
mkdir -p ${OH_TEE_HEADERS_DIR}/libc
mkdir -p ${OH_TEE_HEADERS_DIR}/sys
cp -r ${CHCORE_DIR}/user/chcore-libc/musl-libc/install/include/* ${OH_TEE_HEADERS_DIR}/libc/
cp ${CHCORE_DIR}/user/chcore-libs/sys-libs/libohtee/include/* ${OH_TEE_HEADERS_DIR}/sys/

# go to framework and build it
cd ${OH_TEE_FRAMEWORK_DIR}/build
./build_framework.sh oh_64 ${CHCORE_DIR}/oh_tee /home/tools/llvm/ 10.0.1

# clean chcore
cd ${CHCORE_DIR}
make clean && mkdir -p ramdisk-dir

# copy libc into ramdisk-dir
cd ${CHCORE_DIR}
mkdir -p ramdisk-dir
cd ${CHCORE_DIR}/user/chcore-libc/musl-libc
cp libc_shared.so ${CHCORE_DIR}/ramdisk-dir/libc_shared.so


# compile again to put the apps into ramdisk-dir
cd ${CHCORE_DIR}
cp oh_tee/apps/* ramdisk-dir
make -j$(nproc)
