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

OH_TOP_DIR=$1
TEE_OUT_PATH=${OH_TOP_DIR}/out/tee
TEE_SRC_TMP_PATH=${TEE_OUT_PATH}/src_tmp
TEE_OS_KERNEL_TMP_DIR=${TEE_SRC_TMP_PATH}/tee_os_kernel
TEE_FRAMEWORK_TMP_DIR=${TEE_SRC_TMP_PATH}/tee_os_framework
TEE_BUILD_PATH=${TEE_OS_KERNEL_TMP_DIR}/build

function copy_tee_source()
{
    rm -rf ${TEE_OUT_PATH}
    mkdir -p ${TEE_SRC_TMP_PATH}
    cp -arf ${OH_TOP_DIR}/base/tee/tee_os_kernel ${TEE_SRC_TMP_PATH}/
    cp -arf ${OH_TOP_DIR}/base/tee/tee_os_framework ${TEE_SRC_TMP_PATH}/
}

copy_tee_source

${TEE_BUILD_PATH}/build_tee.sh ${OH_TOP_DIR} ${TEE_OS_KERNEL_TMP_DIR} ${TEE_FRAMEWORK_TMP_DIR}
