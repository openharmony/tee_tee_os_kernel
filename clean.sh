CHCORE_DIR=$(pwd)
OH_TEE_FRAMEWORK_DIR=${CHCORE_DIR}/../tee_os_framework/
OH_TOP_DIR=${CHCORE_DIR}/../../../
THIRD_PARTY=${OH_TOP_DIR}/third_party/

# framework output dir
rm -rf oh_tee

# clean kernel build outputs
make clean

# clean framework build outputs
cd ${OH_TEE_FRAMEWORK_DIR}/build/
./clean_framework.sh
