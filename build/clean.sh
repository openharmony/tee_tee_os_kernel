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
CHCORE_DIR=$2
COMPILER_DIR=$3
COMPILER_VER=$4
if [[ -z "$OH_TOP_DIR" ]]; then
    OH_TOP_DIR=$(pwd)/../../../..
fi
if [[ -z "$CHCORE_DIR" ]]; then
    CHCORE_DIR=$(pwd)/..
fi
OH_TEE_FRAMEWORK_DIR=${CHCORE_DIR}/../tee_os_framework/
THIRD_PARTY=${OH_TOP_DIR}/third_party/

# framework output dir
rm -rf oh_tee

# clean kernel build outputs
cd ${CHCORE_DIR}
make clean

# clean framework build outputs
cd ${OH_TEE_FRAMEWORK_DIR}/build/
./clean_framework.sh ${OH_TEE_FRAMEWORK_DIR}
