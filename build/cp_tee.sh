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
echo "cp_tee.sh start"
OH_TOP_DIR=$1
OUT_DIR=$2
if [[ -z "$OH_TOP_DIR" ]]; then
    OH_TOP_DIR=$(pwd)/../../../..
fi
if [[ -z "$OH_TOP_DIR" ]]; then
    OUT_DIR=$(OH_TOP_DIR)/out/tee
fi
if [[ -z "$OUT_DIR/tee" ]]; then
    mkdir -p $OUT_DIR/tee
fi
rm -rf ${OUT_DIR}/tee_os_kernel
rm -rf ${OUT_DIR}/tee_os_framework
cp -r ${OH_TOP_DIR}/base/tee/tee_os_kernel ${OUT_DIR}
cp -r ${OH_TOP_DIR}/base/tee/tee_os_framework ${OUT_DIR}
