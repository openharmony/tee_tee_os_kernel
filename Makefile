# Copyright (c) 2023 Institute of Parallel And Distributed Systems (IPADS)
# Licensed under the Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#     http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
# PURPOSE.
# See the Mulan PSL v2 for more details.

all: user kernel

Q=@

# libc
LIBC_DIR := $(realpath user/chcore-libc)
libc:
	$(Q)cd $(LIBC_DIR) \
	&& ./configure --prefix=$(LIBC_DIR)/install \
		--syslibdir=$(LIBC_DIR)/lib \
		--target=$(CHCORE_CROSS_COMPILE) \
		COMPILER=$(CHCORE_COMPILER) \
		CROSS_COMPILE=llvm- \
		CFLAGS="$(CONFIG_FLAGS)" \
	&& make -j$(shell nproc) > /dev/null \
	&& make install

# common flags for userspace targets (libs, system servers and apps)
include config.mk
override CC := $(LIBC_DIR)/install/bin/musl-clang
override AR := $(LIBC_DIR)/install/bin/musl-ar
override OBJCOPY := llvm-objcopy
override OBJDUMP := llvm-objdump
override RANLIB := llvm-ranlib
override DEFAULT_CFLAGS = -Wall -O3 $(CONFIG_FLAGS) \
	-I$(realpath user/chcore-libs/sys-interfaces) \
	-I$(realpath user/chcore-libs/sys-libs/libohtee/include)

# flags for libs
# shared and static compile flags
override DEFAULT_LIB_CFLAGS := $(DEFAULT_CFLAGS) -fPIC
# shared link flags
override DEFAULT_SHARED_LIB_LDFLAGS := $(DEFAULT_LDFLAGS)  -fPIC -shared

# flags for system servers and apps
# shared and static compile flags
override DEFAULT_USER_CFLAGS := $(DEFAULT_CFLAGS) -fPIE
# dynamic link flags
override DEFAULT_DYNAMIC_USER_LDFLAGS := $(DEFAULT_LDFLAGS) -pie
# static link flags
override DEFAULT_STATIC_USER_LDFLAGS := $(DEFAULT_LDFLAGS) -static

LIB_DIR := $(realpath user/chcore-libs/sys-libs)
SERVER_DIR := $(realpath user/system-services/system-servers)
ROOT_DIR := $(realpath .)

USER_MAKE_PARAM := CC=$(CC) \
				   AR=$(AR) \
				   OBJCOPY=$(OBJCOPY) \
				   OBJDUMP=$(OBJDUMP) \
				   RANLIB=$(RANLIB) \
				   DEFAULT_LIB_CFLAGS="$(DEFAULT_LIB_CFLAGS)" \
				   DEFAULT_SHARED_LIB_LDFLAGS="$(DEFAULT_SHARED_LIB_LDFLAGS)" \
				   DEFAULT_USER_CFLAGS="$(DEFAULT_USER_CFLAGS)" \
				   DEFAULT_DYNAMIC_USER_LDFLAGS="$(DEFAULT_DYNAMIC_USER_LDFLAGS)" \
				   DEFAULT_STATIC_USER_LDFLAGS="$(DEFAULT_STATIC_USER_LDFLAGS)" \
				   LIB_DIR=$(LIB_DIR) \
				   SERVER_DIR=$(SERVER_DIR) \
				   ROOT_DIR=$(ROOT_DIR) \
				   Q=$(Q)

USER_TARGETS := libfs_base.a libohtee.so
USER_TARGETS += tmpfs.srv chanmgr.srv fsm.srv procmgr
USER_TARGET_DIR_MAP := \
			libohtee.so=$(LIB_DIR)/libohtee \
			libfs_base.a=$(SERVER_DIR)/fs_base \
			tmpfs.srv=$(SERVER_DIR)/tmpfs \
			chanmgr.srv=$(SERVER_DIR)/chanmgr \
			fsm.srv=$(SERVER_DIR)/fsm \
			procmgr=$(SERVER_DIR)/procmgr

.PHONY: ramdisk user $(USER_TARGETS) clean

define target_dir
	$(word 2,$(subst =, ,$(filter $(1)=%,$(USER_TARGET_DIR_MAP))))
endef

# declare dependencies betwee libs, system servers and apps
# ramdisk indicates which targets should be put in the ramdisk
ramdisk: libohtee.so chanmgr.srv
	$(shell mkdir -p ramdisk-dir)
	$(Q)cp $(foreach target,$^,$(call target_dir,$(target))/$(target)) ramdisk-dir

tmpfs.srv: ramdisk libfs_base.a
procmgr: fsm.srv tmpfs.srv

# make all targets
user: $(USER_TARGETS)
$(USER_TARGETS): libc
	make $(USER_MAKE_PARAM) -C $(call target_dir,$@) $@

# kernel
KERNEL_MAKE_PARAM := PROCMGR=$(SERVER_DIR)/procmgr/procmgr \
					 Q=$(Q)

.PHONY: kernel
kernel: procmgr
	$(Q)make $(KERNEL_MAKE_PARAM) -C kernel

# related to clean
clean-user-%:
	make $(USER_MAKE_PARAM) -C $(call target_dir,$*) clean

clean: $(addprefix clean-user-,$(USER_TARGETS))
	$(Q)rm -rf ramdisk-dir
	$(Q)make $(KERNEL_MAKE_PARAM) -C kernel clean
	$(Q)rm -rf $(LIBC_DIR)/install
	$(Q)make -C $(LIBC_DIR) clean

