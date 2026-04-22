# OpenTrustee 可信启动链配置指南

## 1. 概述

可信启动链（Trusted Boot Chain）是指设备从上电开始，到引导程序、内核以及后续系统组件逐级完成加载与校验的信任传递机制。其核心目标是确保各阶段被加载的代码和数据均来源可信、内容完整且未被篡改，从而建立一条不可绕过的启动信任链。

在该机制下，前一阶段负责对后一阶段进行签名校验，只有校验通过后，启动流程才会继续执行。通过这种逐级验证方式，可以有效提升系统在启动阶段的完整性与抗篡改能力，是构建可信执行环境的重要基础。

## 2. 文档目标

本文档旨在指导开发者完成 OpenTrustee 可信启动链的基础配置与验证流程，内容覆盖环境准备、源码修改、镜像构建、密钥生成、镜像签名、镜像烧录及启动验证等关键步骤。

完成本文档中的操作后，读者应能够：

- 理解可信启动链的基本组成与信任传递关系；
- 在 OpenTrustee 方案下完成可信启动相关配置；
- 独立完成镜像签名、构建与烧录；
- 基于启动日志对可信启动结果进行初步验证与问题定位。

## 3. 适用范围与运行环境说明

本文档以 **RK3588 SoC 平台** 作为运行硬件环境进行说明。文中涉及的启动链配置、镜像组织形式和签名流程，均围绕 RK3588 平台的典型实现展开。

需要说明的是，本文档中的方案已在一套基于 RK3588 的开发板环境上完成验证。为增强文档的通用性，后续描述仅在涉及板级差异时提示读者根据实际产品进行调整。实际适配过程中，不同板级平台在以下内容上可能存在差异：

- 板级目录结构；
- 设备树文件名称；
- 启动参数（bootargs）；
- 分区布局与 `parameter.txt`；
- U-Boot、内核及 TEE 的集成方式。

因此，阅读和操作本文档时，请结合自身产品的软件目录结构和板级配置进行校准。

另外需要明确的是，本文档当前内容基于 **OpenHarmony 5.1.0 Release** 版本进行整理，文中涉及的源码路径、脚本位置、配置项及构建流程，均以该版本为参考基线。

## 4. 环境准备

为避免版本差异带来的行为不一致，本文档中的目录组织、代码修改点、构建命令及验证结果，均基于 **OpenHarmony 5.1.0 Release** 进行整理。若你使用的是其他 OpenHarmony 版本，相关脚本路径、补丁位置、配置项名称或镜像组织方式可能存在差异，建议先完成版本对齐后再继续后续操作。

### 4.1 创建工作目录

```bash
mkdir -p ${HOME}/workspace/ohos510
cd ${HOME}/workspace/ohos510
```

### 4.2 获取并编译 OpenHarmony 5.1.0 Release 源码

首先准备 OpenHarmony 5.1.0 Release 源码。在源码根目录下执行如下命令，完成源码初始化、同步、依赖下载及产品编译。其中，`product_name` 需要替换为实际产品名称。

```bash
repo init -u https://gitcode.com/openharmony/manifest.git -b refs/tags/OpenHarmony-v5.1.0-Release --no-repo-verify
repo sync -c -j$(nproc)
repo forall -c 'git lfs pull'
bash build/prebuilts_download.sh
./build.sh --product-name product_name --ccache
```

> [!NOTE]
>
> - 本文档后续所有代码路径与修改点，均以 OpenHarmony 5.1.0 Release 的源码结构为基准；
> - 首次完整编译 OpenHarmony 的目的是确认基础构建环境可用，同时生成后续可信启动链配置过程中会依赖的基础产物；
> - 若编译命令、产品名称或源码目录结构与当前工程不一致，请以实际产品配置为准进行调整。

### 4.3 下载 OpenTrustee 源码

在 OpenHarmony 5.1.0 Release 源码根目录下执行：

```bash
cd ${HOME}/workspace/ohos510/base/tee
git clone https://gitcode.com/openharmony/tee_tee_os_kernel.git tee_os_kernel
git clone https://gitcode.com/openharmony/tee_tee_os_framework.git tee_os_framework
```

### 4.4 下载 `uboot_rk3588` 源码

```bash
cd ${HOME}/workspace
git clone https://gitee.com/hihope-rk3588/uboot_rk3588.git
```

### 4.5 补充 `make.sh` 编译脚本

当前 `uboot_rk3588` 仓库中缺少 `make.sh` 编译脚本，因此需要从相关仓库中补充该脚本，并结合 RK3588 平台的实际构建要求完成调整。

参考仓库：

- `device_rockchip_third_party_uboot`：`https://gitee.com/hihope-rockchip/device_rockchip_third_party_uboot.git`

处理步骤如下：

1. 从上述参考仓库中获取 `make.sh`；
2. 将 `make.sh` 复制到 `uboot_rk3588` 根目录；
3. 为 `make.sh` 增加执行权限；
4. 按本文后续“代码修改”章节中的说明，调整交叉编译器路径、BL32 输入文件等内容。

### 4.6 环境变量说明

建议预先设置以下环境变量，便于统一描述后续路径：

- `BOARD`：产品板级名称，一般与 `device/board/` 下的目录名一致；
- `DEVICE_COMPANY`：设备厂商名称。

设置方式如下：

```bash
export BOARD=<your_board>
export DEVICE_COMPANY=<your_company>
```

### 4.7 目录结构约定

本文档默认使用如下目录结构，后续所有路径说明均基于该约定：

```text
${HOME}/workspace/
├── ohos510/                   # OpenHarmony 5.1.0 Release 源码根目录
└── uboot_rk3588/              # U-Boot 源码目录
```

如你的实际目录与此不一致，请在执行命令、修改脚本或调整补丁路径时同步修正。

## 5. 代码修改

本章节汇总实现可信启动链所需的关键源码修改项。建议按章节顺序逐项处理，并在每一项修改完成后做好版本记录，避免遗漏或因多次试验导致配置不一致。

---

### 5.1 OpenHarmony 5.1.0 Release 相关修改

#### 5.1.1 修改内核 defconfig

文件：

找到当前产品实际使用的内核配置文件，通常命名形式为：

```text
${BOARD}_defconfig
```

修改目的：

- 关闭 `CONFIG_OPTEE`；
- 使能 FIT 启动镜像相关能力；
- 增加 OpenTrustee 运行所需的内核配置项。

修改内容如下：

```diff
@@ -6359,7 +6359,7 @@ CONFIG_TEE=y
 #
 # TEE drivers
 #
-CONFIG_OPTEE=y
+# CONFIG_OPTEE=y
 CONFIG_OPTEE_SHM_NUM_PRIV_PAGES=1
 # end of TEE drivers

@@ -7290,3 +7290,10 @@ CONFIG_CC_HAS_SANCOV_TRACE_PC=y
 # CONFIG_MEMTEST is not set
 # end of Kernel Testing and Coverage
 # end of Kernel hacking
+CONFIG_TZDRIVER=y
+CONFIG_KERNEL_CLIENT=y
+CONFIG_THIRDPARTY_COMPATIBLE=y
+CONFIG_CPU_AFF_NR=1
+CONFIG_TEELOG=y
+CONFIG_SHARED_MEM_RESERVED=y
+CONFIG_FIT=y
```

#### 5.1.2 配置 `tzdriver`

为使内核能够正确识别并加载 `tzdriver`，需要同时完成设备树节点配置以及内核驱动配置项接入。

**1）补充设备树节点**

在目标开发板对应的设备树文件中添加 `trusted_core` 设备节点，例如：

```dts
trusted_core {
    compatible = "trusted_core";
    interrupts = <0 296 4>;
};
```

该节点用于向内核描述 `trusted_core` 设备的基本信息，使 `tzdriver` 能够在启动过程中完成设备匹配与中断注册。

**2）修改驱动配置文件 `Kconfig`**

在内核驱动配置文件中引入 `tzdriver` 的 Kconfig 配置项，添加如下内容：

```makefile
source "drivers/tzdriver/Kconfig"
```

该修改用于将 `tzdriver` 纳入内核配置系统，使其能够通过 `menuconfig` 或 defconfig 参与编译配置。

**3）修改驱动编译文件 `Makefile`**

在内核驱动目录对应的 `Makefile` 中添加 `tzdriver` 编译配置：

```makefile
obj-$(CONFIG_TZDRIVER) += tzdriver/
```

该修改用于在启用 `CONFIG_TZDRIVER` 时，将 `tzdriver` 驱动目录纳入内核编译流程。

完成以上配置后，`tzdriver` 才能在设备树描述、配置系统和编译系统三个层面完成接入，为后续可信启动链相关功能提供基础支撑。


#### 5.1.3 修改 `build_kernel.sh`

文件：

- `${HOME}/workspace/ohos510/device/board/${DEVICE_COMPANY}/${BOARD}/kernel/build_kernel.sh`

修改目的：

- 将编译出的`boot_fit.img`与`boot_linux.img`复制到images目录下；

修改内容如下：

```diff
-cp ${KERNEL_SRC_TMP_PATH}/boot_linux.img ${OUT_PKG_DIR}/boot_linux.img
+if [ -f ${KERNEL_SRC_TMP_PATH}/boot_linux.img ]; then
+    cp ${KERNEL_SRC_TMP_PATH}/boot_linux.img ${OUT_PKG_DIR}/boot_linux.img
+fi
+if [ -f ${KERNEL_SRC_TMP_PATH}/boot_fit.img ]; then
+    cp ${KERNEL_SRC_TMP_PATH}/boot_fit.img ${OUT_PKG_DIR}/boot_fit.img
+fi
```

#### 5.1.4 修改 `make-ohos.sh`

文件：

- `${HOME}/workspace/ohos510/device/board/${DEVICE_COMPANY}/${BOARD}/kernel/make-ohos.sh`

修改目的：

- 在内核镜像和 DTB 生成后，额外打包 `boot_fit.img`；
- 将 ramdisk 一并纳入 FIT 镜像，形成统一的启动镜像。

> [!WARNING]
>
> 在执行该步骤前，需要确保 `ramdisk.img` 已成功生成；否则在 FIT 打包阶段会因输入文件缺失而失败。

修改内容如下：

```diff
diff --git a/${BOARD}/kernel/make-ohos.sh b/${BOARD}/kernel/make-ohos.sh
index e408780..2fabca3 100755
--- a/${BOARD}/kernel/make-ohos.sh
+++ b/${BOARD}/kernel/make-ohos.sh
@@ -113,12 +113,14 @@ function make_boot_linux()
 	make_extlinux_conf ${dtb_path} ${uart} ${image}
 	cp -f ${OBJ_PATH}arch/${arch}/boot/${image} ${EXTLINUX_PATH}/
 	cp -f ${OBJ_PATH}${dtb_path}/${dtb}.dtb ${EXTLINUX_PATH}/${TOYBRICK_DTB}
-	# cp -f logo*.bmp ${BUILD_PATH}/
-  if [ "enable_ramdisk" != "${ramdisk_flag}" ]; then
-		make_ext2_image
-  fi
+	# cp -f logo*.bmp ${BUILD_PATH}/
+	if [ "enable_ramdisk" != "${ramdisk_flag}" ]; then
+		make_ext2_image
+	fi
+
+	./make-fitimage.sh boot_fit.img boot.its \
+		${EXTLINUX_PATH}/${image} \
+		${EXTLINUX_PATH}/${TOYBRICK_DTB} \
+		${EXTLINUX_PATH}/../../../../../packages/phone/images/ramdisk.img
+}
 ramdisk_flag=$2
 found=0
 for i in "${model_list[@]}"; do
@@ -127,4 +129,3 @@ for i in "${model_list[@]}"; do
 		found=1
 	fi
 done
-
```

#### 5.1.5 新增 `make-fitimage.sh`

在 `${BOARD}/kernel/` 目录下新建脚本：

- `${HOME}/workspace/ohos510/device/board/${DEVICE_COMPANY}/${BOARD}/kernel/make-fitimage.sh`

该脚本的主要作用如下：

- 动态替换 `boot.its` 中的 kernel、dtb、ramdisk 路径；
- 调用 `mkimage` 工具生成 `boot_fit.img`；
- 为后续 FIT 签名和统一启动镜像构建提供输入。

脚本内容如下：

```bash
#!/bin/bash

set -e

TOP_DIR=${HOME}/workspace/uboot_rk3588

fdt=0
kernel=0
ramdisk=0
resource=0

OUTPUT_TARGET_IMAGE="$1"
SRC_ITS_FILE="$2"
KERNEL_IMAGE="$3"
KERNEL_DTB_FILE="$4"
RAMDISK_FILE_PATH="$5"
TARGET_ITS_FILE=".tmp_its"

if [ ! -f "${SRC_ITS_FILE}" ]; then
  echo "Not Found: ${SRC_ITS_FILE}"
  exit 1
fi

if [ ! -f "${RAMDISK_FILE_PATH}" ]; then
  echo "Not Found: ${RAMDISK_FILE_PATH}"
  exit 1
fi

if [ ! -f "${KERNEL_DTB_FILE}" ]; then
  echo "Not Found: ${KERNEL_DTB_FILE}"
  exit 1
fi

if [ ! -f "${KERNEL_IMAGE}" ]; then
  echo "Not Found: ${KERNEL_IMAGE}"
  exit 1
fi

if [ ! -f "${TOP_DIR}/rkbin/tools/mkimage" ]; then
  echo "Not Found: ${TOP_DIR}/rkbin/tools/mkimage"
  exit 1
fi

rm -f "${TARGET_ITS_FILE}"
mkdir -p "$(dirname "${TARGET_ITS_FILE}")"

while read -r line
do
  if [ ${fdt} -eq 1 ]; then
    echo "data = /incbin/(\"${KERNEL_DTB_FILE}\");" >> "${TARGET_ITS_FILE}"
    fdt=0
    continue
  fi
  if echo "${line}" | grep -w "^fdt" | grep -v ";" >/dev/null; then
    fdt=1
    echo "${line}" >> "${TARGET_ITS_FILE}"
    continue
  fi

  if [ ${kernel} -eq 1 ]; then
    echo "data = /incbin/(\"${KERNEL_IMAGE}\");" >> "${TARGET_ITS_FILE}"
    kernel=0
    continue
  fi
  if echo "${line}" | grep -w "^kernel" | grep -v ";" >/dev/null; then
    kernel=1
    echo "${line}" >> "${TARGET_ITS_FILE}"
    continue
  fi

  if [ -f "${RAMDISK_FILE_PATH}" ]; then
    if [ ${ramdisk} -eq 1 ]; then
      echo "data = /incbin/(\"${RAMDISK_FILE_PATH}\");" >> "${TARGET_ITS_FILE}"
      ramdisk=0
      continue
    fi
    if echo "${line}" | grep -w "^ramdisk" | grep -v ";" >/dev/null; then
      ramdisk=1
      echo "${line}" >> "${TARGET_ITS_FILE}"
      continue
    fi
  fi

  if [ ${resource} -eq 1 ]; then
    echo "data = /incbin/(\"boot_linux.img\");" >> "${TARGET_ITS_FILE}"
    resource=0
    continue
  fi
  if echo "${line}" | grep -w "^resource" | grep -v ";" >/dev/null; then
    resource=1
    echo "${line}" >> "${TARGET_ITS_FILE}"
    continue
  fi

  if [ "${RK_RAMDISK_SECURITY_BOOTUP}" = "true" ]; then
    if echo "${line}" | grep -wq "uboot-ignore"; then
      echo "Enable Security boot, skip uboot-ignore ..."
      continue
    fi
  fi

  echo "${line}" >> "${TARGET_ITS_FILE}"
done < "${SRC_ITS_FILE}"

"${TOP_DIR}/rkbin/tools/mkimage" -f "${TARGET_ITS_FILE}" -E -p 0x800 "${OUTPUT_TARGET_IMAGE}"
rm -f "${TARGET_ITS_FILE}"
```

> [!WARNING]
>
> - 这里的 `TOP_DIR=${HOME}/workspace/uboot_rk3588` 仅为示例，请根据实际工程目录进行调整；
> - 如果 `rkbin/tools/mkimage` 不在该路径下，脚本将无法生成 `boot_fit.img`。

#### 5.1.5 修改 `boot.its`

文件：

- `${HOME}/workspace/ohos510/kernel/linux/<kernel_version>/boot.its`

其中，`<kernel_version>` 为实际使用的内核版本，例如 `linux-5.10`。

修改目的：

- 将 FIT 镜像中的 `resource` 调整为 `ramdisk`；
- 明确指定 kernel 和 dtb 的描述、架构及属性；
- 将签名对象覆盖到 `fdt`、`kernel` 和 `ramdisk`，形成完整校验链路。

修改内容如下：

```diff
diff --git a/boot.its b/boot.its
index 755005c42..d52cce43e 100644
--- a/boot.its
+++ b/boot.its
@@ -10,9 +10,10 @@

     images {
         fdt {
-            data = /incbin/("fdt");
+            data = /incbin/("boot_linux/extlinux/toybrick.dtb");
+            description = "toybrick.dtb";
             type = "flat_dt";
-            arch = "";
+            arch = "arm64";
             compression = "none";
             load = <0xffffff00>;

@@ -22,11 +23,12 @@
         };

         kernel {
-            data = /incbin/("kernel");
+            data = /incbin/("boot_linux/extlinux/Image");
+            description = "kernel Image";
             type = "kernel";
-            arch = "";
+            arch = "arm64";
             os = "linux";
-            compression = "";
+            compression = "none";
             entry = <0xffffff01>;
             load = <0xffffff01>;

@@ -35,12 +37,14 @@
             };
         };

-        resource {
-            data = /incbin/("resource");
-            type = "multi";
-            arch = "";
+        ramdisk {
+            data = /incbin/("../../../packages/phone/images/ramdisk.img");
+            os = "linux";
+            type = "ramdisk";
             compression = "none";
-
+            
+            load = <0xffffff02>;
+            arch = "arm64";
             hash {
                 algo = "sha256";
             };
@@ -54,13 +58,13 @@
             rollback-index = <0x00>;
             fdt = "fdt";
             kernel = "kernel";
-            multi = "resource";
-
+            ramdisk = "ramdisk";
+            
             signature {
                 algo = "sha256,rsa2048";
                 padding = "pss";
                 key-name-hint = "dev";
-                sign-images = "fdt", "kernel", "multi";
+                sign-images = "fdt", "kernel", "ramdisk";
             };
         };
     };
```

---

### 5.2 OpenTrustee 相关代码修改

#### 5.2.1 修改 `config.mk`

文件：

- `${HOME}/workspace/ohos510/base/tee/tee_os_kernel/config.mk`

修改目的：

- 将目标平台从 `rk3568` 切换为 `rk3588`，与当前运行环境保持一致。

修改内容如下：

```diff
diff --git a/config.mk b/config.mk
index 0b6f6f9..f945bf3 100644
--- a/config.mk
+++ b/config.mk
@@ -12,7 +12,7 @@
 STR_CONFIGS = \
 CHCORE_COMPILER=clang \
 CHCORE_CROSS_COMPILE=aarch64-linux-ohos- \
-CHCORE_PLAT=rk3568 \
+CHCORE_PLAT=rk3588 \
 CHCORE_ARCH=aarch64 \
 CHCORE_SPD=opteed
```

#### 5.2.2 修改 `build_tee.sh`

文件：

- `${HOME}/workspace/ohos510/base/tee/tee_os_kernel/build/build_tee.sh`

修改目的：

- 在构建 framework 时将目标平台指定为 `rk3588`，确保 TEE 侧产物与硬件平台匹配。

修改内容如下：

```diff
diff --git a/build/build_tee.sh b/build/build_tee.sh
index b627764..35aaba0 100755
--- a/build/build_tee.sh
+++ b/build/build_tee.sh
@@ -61,7 +61,7 @@ cp -r ${CHCORE_DIR}/user/chcore-libc/musl-libc/install/include/* ${OH_TEE_HEADER
 cp ${CHCORE_DIR}/user/chcore-libs/sys-libs/libohtee/include/* ${OH_TEE_HEADERS_DIR}/sys/
 # go to framework and build it
 cd ${OH_TEE_FRAMEWORK_DIR}/build
-./build_framework.sh oh_64 ${CHCORE_DIR}/oh_tee ${COMPILER_DIR} ${COMPILER_VER} ${OH_TEE_FRAMEWORK_DIR} ${THIRD_PARTY} rk3568
+./build_framework.sh oh_64 ${CHCORE_DIR}/oh_tee ${COMPILER_DIR} ${COMPILER_VER} ${OH_TEE_FRAMEWORK_DIR} ${THIRD_PARTY} rk3588
 # compile again to put the apps into ramdisk-dir
 cd ${CHCORE_DIR}
 make clean
```

---

### 5.3 U-Boot 相关代码修改

#### 5.3.1 修改 `rk3588_defconfig`

文件：

- `${HOME}/workspace/uboot_rk3588/configs/rk3588_defconfig`

修改目的：

- 关闭 OP-TEE Client；
- 使能 FIT 签名及多镜像支持；
- 为可信启动链提供 U-Boot 侧基础能力。

修改内容如下：

```diff
diff --git a/configs/rk3588_defconfig b/configs/rk3588_defconfig
index fd6c911..364613c 100644
--- a/configs/rk3588_defconfig
+++ b/configs/rk3588_defconfig
@@ -220,6 +220,10 @@ CONFIG_AVB_LIBAVB_AB=y
 CONFIG_AVB_LIBAVB_ATX=y
 CONFIG_AVB_LIBAVB_USER=y
 CONFIG_RK_AVB_LIBAVB_USER=y
-CONFIG_OPTEE_CLIENT=y
+# CONFIG_OPTEE_CLIENT=y
 CONFIG_OPTEE_V2=y
 CONFIG_OPTEE_ALWAYS_USE_SECURITY_PARTITION=y
+CONFIG_SPL_FIT_IMAGE_KB=8192
+CONFIG_SPL_FIT_IMAGE_MULTIPLE=1
+CONFIG_FIT_SIGNATURE=y
+CONFIG_SPL_FIT_SIGNATURE=y
```

#### 5.3.2 修改 `bootfit.c`

文件：

- `${HOME}/workspace/uboot_rk3588/cmd/bootfit.c`

修改目的：

- 在启动 FIT 镜像前，显式设置适用于 OpenHarmony 的 `bootargs`；
- 确保内核启动阶段能够获取正确的控制台、存储设备、分区挂载及系统启动参数。

> [!NOTE]
>
> 本节中的 `bootargs` 属于板级实现相关内容，实际产品中应根据设备树、存储介质类型、分区布局、挂载路径及启动介质进行调整，不建议直接照搬。
>
> 除在 `bootfit.c` 中显式设置 `bootargs` 外，也可以将启动参数配置在对应设备的设备树中，例如放在 `/chosen` 节点的 `bootargs` 属性中统一维护。该方式更适合板级差异较大的产品形态，便于将启动参数与设备硬件配置统一管理，降低后续维护成本。
>
> 但需要注意，若当前启动流程中仍在 `bootfit.c` 里通过 `env_update("bootargs", ...)` 显式更新启动参数，则最终生效的内容通常以 U-Boot 在启动阶段写入的 `bootargs` 为准。因此，如果计划改为在设备树中维护启动参数，应同步评估并调整 `bootfit.c` 中的相关逻辑，避免设备树配置与 U-Boot 运行时参数相互覆盖。
>
> 总体而言，二者都可以作为启动参数的配置入口：
>
> - 若需要快速验证、集中控制启动参数，建议在 `bootfit.c` 中设置；
> - 若希望按设备型号进行板级管理，建议在对应设备的设备树中维护。

修改内容如下：

```diff
diff --git a/cmd/bootfit.c b/cmd/bootfit.c
index a0efec7..de84b9a 100644
--- a/cmd/bootfit.c
+++ b/cmd/bootfit.c
@@ -15,6 +15,19 @@

 DECLARE_GLOBAL_DATA_PTR;

+static const char ohos_fit_bootargs[] =
+	"earlycon=uart8250,mmio32,0xfeb50000 "
+	"console=ttyFIQ0 "
+	"root=PARTUUID=614e0000-0000 "
+	"default_boot_device=fe2e0000.mmc "
+	"rw rootwait "
+	"ohos.boot.sn=/sys/block/mmc1/device/cid "
+	"ohos.required_mount.system=/dev/block/platform/fe2e0000.mmc/by-name/system@/usr@ext4@ro,barrier=1@wait,required "
+	"ohos.required_mount.vendor=/dev/block/platform/fe2e0000.mmc/by-name/vendor@/vendor@ext4@ro,barrier=1@wait,required "
+	"ohos.required_mount.misc=/dev/block/platform/fe2e0000.mmc/by-name/misc@none@none@none@wait,required "
+	"ohos.required_mount.bootctrl=/dev/block/platform/fe2e0000.mmc/by-name/bootctrl@none@none@none@wait,required";
+
 static void *do_boot_fit_storage(ulong *size)
 {
 	return fit_image_load_bootables(size);
@@ -93,6 +106,7 @@ static int do_boot_fit(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
 	bootm_args[0] = fit_addr;

 	printf("at %s with size 0x%08lx\n", fit_addr, size);
+	env_update("bootargs", ohos_fit_bootargs);

 #ifdef CONFIG_ANDROID_AB
 	char slot_suffix[3] = {0};
```

#### 5.3.3 修改 `make.sh`

文件：

- `${HOME}/workspace/uboot_rk3588/make.sh`

修改目的：

- 指定 ARM32 / ARM64 交叉编译器路径；
- 修正 `BL32_BIN` 的取值逻辑，确保打包阶段引用到正确的 `bl32.bin`。

请根据本地 OpenHarmony 工具链安装位置进行调整，示例如下：

```diff
RKBIN_TOOLS=./rkbin/tools
CROSS_COMPILE_ARM32=../ohos510/prebuilts/gcc/linux-x86/arm/gcc-linaro-7.5.0-arm-linux-gnueabi/bin/arm-linux-gnueabihf-
CROSS_COMPILE_ARM64=../ohos510/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-

-BL32_BIN=`sed -n '/_bl32_/s/PATH=//p' ${INI} | tr -d '\r'`
+BL32_BIN=`sed -n '/bl32/s/PATH=//p' ${INI} | tr -d '\r'`
```

#### 5.3.4 修改 `fit-core.sh`

文件：

- `${HOME}/workspace/uboot_rk3588/scripts/fit-core.sh`

修改目的：

- 修正 `rk_sign_tool` 的相对路径，确保签名流程可正常执行。

修改内容如下：

```diff
diff --git a/scripts/fit-core.sh b/scripts/fit-core.sh
index ecfd40d..f3e5823 100644
--- a/scripts/fit-core.sh
+++ b/scripts/fit-core.sh
@@ -27,7 +27,7 @@ KERNEL_ADDR_PLACEHOLDER="0xffffff01"
 RAMDISK_ADDR_PLACEHOLDER="0xffffff02"
 # tools
 MKIMAGE="./tools/mkimage"
-RK_SIGN_TOOL="../rkbin/tools/rk_sign_tool"
+RK_SIGN_TOOL="./rkbin/tools/rk_sign_tool"
 FIT_UNPACK="./scripts/fit-unpack.sh"
 CHECK_SIGN="./tools/fit_check_sign"
```

#### 5.3.5 修改 `rk3588_common.h`

文件：

- `${HOME}/workspace/uboot_rk3588/include/configs/rk3588_common.h`

修改目的：

- 调整 FDT 加载地址，避免启动阶段出现地址冲突。

修改内容如下：

```diff
diff --git a/include/configs/rk3588_common.h b/include/configs/rk3588_common.h
index de25351..aef7e9f 100644
--- a/include/configs/rk3588_common.h
+++ b/include/configs/rk3588_common.h
@@ -62,7 +62,7 @@
 #define ENV_MEM_LAYOUT_SETTINGS \
 	"scriptaddr=0x00500000\0" \
 	"pxefile_addr_r=0x00600000\0" \
-	"fdt_addr_r=0x0a100000\0" \
+	"fdt_addr_r=0x08300000\0" \
 	"kernel_addr_r=0x00400000\0" \
 	"kernel_addr_c=0x05480000\0" \
 	"ramdisk_addr_r=0x0a200000\0"
```

#### 5.3.6 修改 `RK3588TRUST.ini`

文件：

- `${HOME}/workspace/uboot_rk3588/rkbin/RKTRUST/RK3588TRUST.ini`

修改目的：

- 将 Trust 镜像中的 `BL32` 输入切换为自行编译生成的 `bl32.bin`。

修改内容如下：

```diff
diff --git a/rkbin/RKTRUST/RK3588TRUST.ini b/rkbin/RKTRUST/RK3588TRUST.ini
index 9c1c200..6b4ffb8 100644
--- a/rkbin/RKTRUST/RK3588TRUST.ini
+++ b/rkbin/RKTRUST/RK3588TRUST.ini
@@ -9,7 +9,7 @@ PATH=bin/rk35/rk3588_bl31_v1.22.elf
 ADDR=0x00040000
 [BL32_OPTION]
 SEC=1
-PATH=bin/rk35/rk3588_bl32_v1.09.bin
+PATH=../../ohos510/base/tee/tee_os_kernel/kernel/bl32.bin
 ADDR=0x08400000
 [BL33_OPTION]
 SEC=0
```

## 6. 编译与签名

### 6.1 生成签名密钥

在 U-Boot 目录下执行以下命令：

```bash
cd ${HOME}/workspace/uboot_rk3588
./rkbin/tools/rk_sign_tool cc --chip 3588
./rkbin/tools/rk_sign_tool kk --bits 2048 --out .
mkdir -p keys
mv privateKey.pem keys/dev.key
mv publicKey.pem keys/dev.pubkey
openssl req -batch -new -x509 -key keys/dev.key -out keys/dev.crt
```

#### 6.1.1 常见异常说明

若执行过程中出现如下错误：

```bash
./rkbin/tools/rk_sign_tool kk --bits 2048 --out .
********sign_tool ver 1.1********
bits is 2048
Segmentation fault (core dumped)
```

通常说明当前 `rk_sign_tool` 版本过低，需要替换为更新版本后重新生成密钥。示例如下：

```bash
git clone https://github.com/rockchip-linux/rkbin.git /tmp/rkbin
cp -rf /tmp/rkbin/tools/ ./rkbin/

./rkbin/tools/rk_sign_tool cc --chip 3588
./rkbin/tools/rk_sign_tool kk --bits 2048 --out .
mkdir -p keys
mv private_key.pem keys/dev.key
mv public_key.pem keys/dev.pubkey
openssl req -batch -new -x509 -key keys/dev.key -out keys/dev.crt
```

执行完成后，检查 `keys/` 目录：

```bash
ls keys/
```

正常情况下应包含以下三个文件：

- `dev.crt`
- `dev.key`
- `dev.pubkey`

> [!CAUTION]
>
> - 上述文件是后续 FIT 镜像签名所必需的关键材料；
> - 私钥 `dev.key` 需要妥善保存；
> - 如果私钥丢失，后续将无法继续生成可被当前信任链接受的新签名镜像。

### 6.2 打包 `boot_fit.img`

#### 6.2.1 重新编译 OpenHarmony

```bash
cd ${HOME}/workspace/ohos510
rm -rf out/${BOARD}/kernel
./build.sh --product-name ${BOARD} --ccache --fast-rebuild
```

#### 6.2.2 检查构建产物

编译完成后，建议确认以下结果：

1. 在编译产物目录下生成了 `boot_fit.img`；
2. `boot_fit.img` 已被复制到 `out/${BOARD}/packages/phone/images/` 目录中。

如果没有生成 `boot_fit.img`，请重点检查以下内容：

- `make-ohos.sh` 是否已正确修改；
- `make-fitimage.sh` 是否已创建并赋予执行权限；
- `boot.its` 是否已按预期调整；
- `mkimage` 路径是否可用。

### 6.3 编译 `bl32.bin`

#### 6.3.1 运行编译脚本

```bash
cd ${HOME}/workspace/ohos510/base/tee/tee_os_kernel/build
./build_tee.sh
```

#### 6.3.2 检查编译结果

建议重点确认以下目录和文件：

- `base/tee/tee_os_kernel/kernel/bl32.bin`
- `base/tee/tee_os_kernel/ramdisk-dir/`

### 6.4 编译 U-Boot

#### 6.4.1 执行编译命令

**在编译前，请手动把前文编译出的boot_fit.img复制到ohos510/out/${BOARD}/packages/phone/images/目录下，如果不复制也可以将下述的boot_fit.img路径替换成实际路径。**

```bash
cd ${HOME}/workspace/uboot_rk3588
./make.sh rk3588 --spl-new --boot_img ../ohos510/out/${BOARD}/packages/phone/images/boot_fit.img
```

#### 6.4.2 将公钥 Hash 烧录至 OTP

> [!CAUTION]
>
> OTP 区域通常仅支持一次性烧录，烧录后不可修改。
> 因此，在开发调试阶段建议先跳过公钥 Hash 烧录，仅完成签名镜像构建与启动验证；待方案稳定并进入量产阶段后，再执行 OTP 烧录操作。

修改 `u-boot/arch/arm/mach-rockchip/fit_misc.c`，使 `fit_board_verify_required_sigs()` 在对应场景下返回 `1`，从而强制执行签名校验。

修改内容如下：

```diff
diff --git a/arch/arm/mach-rockchip/fit_misc.c b/arch/arm/mach-rockchip/fit_misc.c
index baff27a21e..614fcb1afc 100644
--- a/arch/arm/mach-rockchip/fit_misc.c
+++ b/arch/arm/mach-rockchip/fit_misc.c
@@ -7,6 +7,7 @@
 #include <common.h>
 #include <boot_rkimg.h>
 #include <misc.h>
+#include <asm/arch/rk_atags.h>
 #ifdef CONFIG_SPL_BUILD
 #include <spl.h>
 #endif
@@ -225,6 +226,17 @@ int fit_board_verify_required_sigs(void)
        printf("Can't read verified-boot flag, ret=%d\n", ret);
        return 1;
    }
+#else
+#ifdef CONFIG_ROCKCHIP_PRELOADER_ATAGS
+   struct tag *t;
+
+   t = atags_get_tag(ATAG_PUB_KEY);
+   if (t) {
+       /* Pass if efuse/otp programmed */
+       if (t->u.pub_key.flag == PUBKEY_FUSE_PROGRAMMED)
+           vboot = 1;
+   }
+#endif
 #endif
 #endif /* CONFIG_SPL_BUILD*/
```

将公钥 Hash 烧录到 OTP 的动作由 loader 完成，因此在编译 U-Boot 时需要在命令中加入 `--burn-key-hash`：

```bash
./make.sh rk3588 --spl-new --boot_img ../ohos510/out/${BOARD}/packages/phone/images/boot_fit.img --burn-key-hash
```

#### 6.4.3 预期输出

编译成功后，终端中通常会看到类似输出：

```text
pack loader with new: spl/u-boot-spl.bin
spl/u-boot-spl.dtb: burn-key-hash=1
...
Image(signed, version=0):  uboot.img (FIT with uboot, trust...) is ready
Image(signed, version=0):  boot.img (FIT with kernel, fdt, resource...) is ready
Image(signed):  rk3588_spl_loader_v1.07.111 (with spl, ddr, usbplug) is ready
```

编译完成后，应至少得到以下文件：

- `boot.img`
- `uboot.img`
- `rk358x_spl_loader_*.bin`

至此，即得到了一套可用于启动验证的签名镜像。

## 7. 烧录

### 7.1 烧录准备

烧录前，需要先准备与当前产品匹配的 `parameter.txt`，并将前述生成的镜像替换到对应分区映像中。

建议替换关系如下：

- 使用 `rk358x_spl_loader_*.bin` 替换 `MiniLoaderAll.bin`
- 使用 `uboot.img` 替换原始 `uboot.img`
- 使用 `boot.img` 替换原始 `boot_linux.img`

> [!NOTE]
>
> - 新的 `boot.img` 中已经包含 `kernel`、`dtb` 以及 `ramdisk`；
> - 因此在实际烧录时，通常无需再单独烧录 `ramdisk.img`。

可参考的 `parameter.txt` 内容如下：

```text
FIRMWARE_VER: 12.0
MACHINE_MODEL: rk3588_carrier
MACHINE_ID: 007
MANUFACTURER: rockchip
MAGIC: 0x5041524B
ATAG: 0x00200800
MACHINE: rk3588_carrier
CHECK_MASK: 0x80
PWR_HLD: 0,0,A,0,1
TYPE: GPT
CMDLINE:mtdparts=rk29xxnand:0x00004000@0x00002000(uboot),0x00002000@0x00006000(misc),0x00001000@0x00008000(bootctrl),0x00003000@0x00009000(resource),0x00030000@0x0000C000(boot:bootable),0x00002000@0x0003C000(ramdisk),0x00400000@0x0003E000(system),0x00200000@0x0043E000(vendor),0x00019000@0x0063E000(sys-prod),0x00019000@0x00657000(chip-prod),0x00010000@0x00670000(updater),0x00008000@0x00680000(eng_system),0x00008000@0x00688000(eng_chipset),0x00020000@0x0069E000(chip_ckm),-@0x01308000(userdata:grow)
uuid:system=614e0000-0000-4b53-8000-1d28000054a9
uuid:boot=a2d37d82-51e0-420d-83f5-470db993dd35
```

> [!WARNING]
>
> `parameter.txt` 中的分区布局与命名具有明显的平台和产品相关性，实际使用时请务必以当前产品的存储方案为准。

## 8. 启动验证

烧录完成后，连接串口并观察 U-Boot 启动日志。

如果流程正确，通常可以看到 FIT 镜像被识别、签名校验通过，并依次完成 kernel、ramdisk 和 fdt 的加载，类似日志如下：

```text
Hit key to stop autoboot('CTRL+C'):  0
## Booting FIT Image at 0xe8ddff00 with size 0x02a19800
Fdt Ramdisk skip relocation
## Loading kernel from FIT Image at e8ddff00 ...
   Using 'conf' configuration
## Verified-boot: 1
   Verifying Hash Integrity ... sha256,rsa2048:dev+ OK
   Trying 'kernel' kernel subimage
     Description:  kernel Image
     Type:         Kernel Image
     Compression:  uncompressed
     Data Start:   0xe8e25500
     Data Size:    41191432 Bytes = 39.3 MiB
     Architecture: AArch64
     OS:           Linux
     Load Address: 0x00400000
     Entry Point:  0x00400000
     Hash algo:    sha256
     Hash value:   38ca39b4aa6dd5bb16044e2f0ed95d1b75f2a1224ff9068cc9753d5cd4ce6385
   Verifying Hash Integrity ... sha256+ OK
## Loading ramdisk from FIT Image at e8ddff00 ...
   Using 'conf' configuration
   Trying 'ramdisk' ramdisk subimage
     Description:  unavailable
     Type:         RAMDisk Image
     Compression:  uncompressed
     Data Start:   0xeb56df00
     Data Size:    2668306 Bytes = 2.5 MiB
     Architecture: AArch64
     OS:           Linux
     Load Address: 0x0a200000
     Entry Point:  unavailable
     Hash algo:    sha256
     Hash value:   9508041c7ef773275466d938406bf21ffb650cf1c2392fb95da2d62cc7f70995
   Verifying Hash Integrity ... sha256+ OK
   Loading ramdisk from 0xeb56df00 to 0x0a200000
## Loading fdt from FIT Image at e8ddff00 ...
   Using 'conf' configuration
   Trying 'fdt' fdt subimage
     Description:  toybrick.dtb
     Type:         Flat Device Tree
     Compression:  uncompressed
     Data Start:   0xe8de0f00
     Data Size:    279983 Bytes = 273.4 KiB
     Architecture: AArch64
     Load Address: 0x08300000
     Hash algo:    sha256
     Hash value:   f724233338d363ea9f89dd1a0e25f9ef0d8a5f34e096865edd1cf665edfd182a
   Verifying Hash Integrity ... sha256+ OK
   Loading fdt from 0x08300000 to 0x08300000
   Booting using the fdt blob at 0x08300000
   Loading Kernel Image from 0xe8e25500 to 0x00400000 ... OK
   kernel loaded at 0x00400000, end = 0x02b48808
  'reserved-memory' cma: addr=10000000 size=10000000
  'reserved-memory' ramoops@110000: addr=110000 size=f0000
   Using Device Tree in place at 0000000008300000, end 0000000008347321
No file: logo_kernel.bmp
Adding bank: 0x00200000 - 0x08400000 (size: 0x08200000)
Adding bank: 0x0a000000 - 0xf0000000 (size: 0xe6000000)
Adding bank: 0x100000000 - 0x3fc000000 (size: 0x2fc000000)
Adding bank: 0x3fc500000 - 0x3fff00000 (size: 0x03a00000)
Total: 3692.712 ms

Starting kernel ...
```

出现以下日志时，说明 FIT 镜像中的关键子镜像已经通过签名或完整性校验，可信启动链配置基本生效：

- **内核校验通过**

  ```text
  Trying 'kernel' kernel subimage
  Verifying Hash Integrity ... sha256+ OK
  ```

- **Ramdisk 校验通过**

  ```text
  Trying 'ramdisk' ramdisk subimage
  Verifying Hash Integrity ... sha256+ OK
  ```

- **FDT 校验通过**

  ```text
  Trying 'fdt' fdt subimage
  Verifying Hash Integrity ... sha256+ OK
  ```

当上述校验链路均符合预期时，可以认为当前镜像已具备可信启动能力，可信启动流程已成功打通。
