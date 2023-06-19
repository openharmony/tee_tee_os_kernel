# Copyright (c) 2023 Institute of Parallel And Distributed Systems (IPADS)
# Licensed under the Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#     http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
# PURPOSE.
# See the Mulan PSL v2 for more details.

CHCORE_DIR=$(pwd)
OH_TEE_FRAMEWORK_DIR=${CHCORE_DIR}/../openharmony/base/tee/tee_tee_os_framework/
OH_TEE_PREBUILD_DIR=${OH_TEE_FRAMEWORK_DIR}/prebuild/
OH_TEE_HEADERS_DIR=${OH_TEE_PREBUILD_DIR}/headers/


# compile chcore
make clean && make -j$(nproc)

rm -rf oh_tee
rm -rf ramdisk-dir

# link libc with bounds checking functions and libtee
# and copy to framework
cd user/chcore-libc
make libc_shared
mkdir -p ${OH_TEE_FRAMEWORK_DIR}/output/aarch64/libs/
cp libc_shared.so ${OH_TEE_FRAMEWORK_DIR}/output/aarch64/libs/libc_shared.so
mkdir -p ${OH_TEE_FRAMEWORK_DIR}/output/aarch64
cp libc_shared.so ${OH_TEE_FRAMEWORK_DIR}/output/aarch64/libc_shared.so

# go to framework and build it
cd ${OH_TEE_FRAMEWORK_DIR}/build
./build_framework.sh

# clean chcore
cd ${CHCORE_DIR}
make clean && mkdir -p ramdisk-dir

# copy libc into ramdisk-dir
cd ${CHCORE_DIR}
mkdir ramdisk-dir
cd ${CHCORE_DIR}/user/chcore-libc
cp libc_shared.so ${CHCORE_DIR}/ramdisk-dir/libc_shared.so

# compile again to put the apps into ramdisk-dir
cd ${CHCORE_DIR}
cp oh_tee/apps/* ramdisk-dir
make -j$(nproc)
