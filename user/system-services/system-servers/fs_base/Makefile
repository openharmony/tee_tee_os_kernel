# Copyright (c) 2023 Institute of Parallel And Distributed Systems (IPADS), Shanghai Jiao Tong University (SJTU)
# Licensed under the Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#     http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
# PURPOSE.
# See the Mulan PSL v2 for more details.

CFLAGS := $(DEFAULT_LIB_CFLAGS)
SRCS := $(wildcard *.c)
OBJS := $(SRCS:%=%.o)
DEPS := $(OBJS:.o=.d)

libfs_base.a: $(OBJS)
	$(Q)$(AR) rc $@ $(OBJS)
	$(Q)$(RANLIB) $@

clean:
	$(Q)rm -f $(OBJS) $(DEPS) libfs_base.a

-include $(DEPS)

$(OBJS): %.o: %
	$(Q)$(CC) $(CFLAGS) -c $< -o $@ -MMD
