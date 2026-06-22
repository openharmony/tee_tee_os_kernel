# tee_tee_os_kernel #

## Introduction ##

OpenTrustee provides a Trusted Execution Environment (TEE) that runs in a secure area isolated by hardware. OpenTrustee is a complete TEE solution that includes multiple components. The system architecture is shown in the figure below:

![](figures/overview-of-opentrustee.png)

The tee_tee_os_kernel component mainly includes the kernel part of OpenTrustee, designed using a microkernel architecture.

## The specific module introduction of tee_tee_os_kernel ##
<table>
<th> Name of module </th>
<th> Introduction </th>
<tr>
<td> kernel/ipc </td><td> inter-process communication </td>
</tr><tr>
<td> kernel/irq </td><td> interrupt handling </td>
</tr><tr>
<td> kernel/mm </td><td> memory management </td>
</tr><tr>
<td> kernel/object </td><td> kernel object management </td>
</tr><tr>
<td> kernel/sched </td><td> thread scheduling </td>
</tr><tr>
<td> user/chcore-libs/sys-libs/libohtee </td><td> library functions that the framework depends on </td>
</tr><tr>
<td> user/system-services/system-servers/procmgr </td><td> process management </td>
</tr><tr>
<td> user/system-services/system-servers/fs_base </td><td> virtual file system </td>
</tr><tr>
<td> user/system-services/system-servers/fsm </td><td> file system management </td>
</tr><tr>
<td> user/system-services/system-servers/tmpfs </td><td> in-memory file system </td>
</tr><tr>
<td> user/system-services/system-servers/chanmgr </td><td> handle naming, indexing, and distribution of channels </td>
</tr>


</table>

## tee_tee_os_kernel code directories ##
```
base/tee/tee_os_kernel
├── kernel
│   ├── arch
│   ├── ipc
│   ├── irq
│   ├── lib
│   ├── mm
│   ├── object
│   ├── sched
│   └── syscall
├── tool
│   └── read_procmgr_elf_tool
├── user/chcore-libs
│   ├── sys-interfaces/chcore-internal
│   └── sys-libs/libohtee
└── user/system-services/system-servers
    ├── chanmgr
    ├── fs_base
    ├── fsm
    ├── procmgr
    └── tmpfs
```

## tee_tee_os_kernel building guide ##

The tee_tee_os_framework and tee_tee_os_kernel work together to build TEEOS, and the commands for building them separately are as follows:

```Bash
./build.sh --product-name rk3568 --build-target tee --ccache
```

Build the product as a TEEOS image:` base/tee/tee_os_kernel/kernel/bl32.bin`

### Enable TEE-side LLM inference support ###

TEE-side LLM inference support is currently available only on RK3588. Before using this feature, make sure `config.mk` sets both:

```Makefile
CHCORE_PLAT=rk3588
CHCORE_LLM=ON
```

If `CHCORE_PLAT` is not `rk3588`, setting `CHCORE_LLM=ON` does not enable the RKNPU syscalls, kernel driver, user-space interfaces, or LLM build and packaging flow.

When enabled, `build/build_tee.sh` builds `oh-llama.cpp` as part of the TEEOS build and copies the inference-related shared libraries into `ramdisk-dir`. Make sure `oh-llama.cpp` is placed next to the `tee_os_kernel` directory. Otherwise, the build script cannot enter the source directory after this feature is enabled.

When this feature is enabled, the REE-side Linux device tree must reserve the physical memory used by TEE-side LLM inference and mark these regions as `no-map`. This prevents the REE kernel, CMA, or user processes from allocating or mapping memory that is owned by the TEE. With the current RK3588 configuration, the memory ranges that must be reserved on the REE side are:

| Start address | End address | Usage |
| --- | --- | --- |
| `0x02e00000` | `0x08000000` | TEE-side inference memory |
| `0x20000000` | `0x50000000` | RKNPU IOMMU page table area and TEE-side inference memory |
| `0x60000000` | `0xC0000000` | TEE-side inference memory |
| `0x400000000` | `0x700000000` | High-address memory for TEE-side inference, only applicable to boards with 32GB memory |

These ranges correspond to the TEE-side `secure_ddr_region()` configuration. The range from `0x20000000` to `0x24000000` is used for RKNPU IOMMU page tables, and the following range from `0x24000000` to `0x50000000` is used as TEE-side inference memory. The high-address range from `0x400000000` to `0x700000000` is above the 16GB address boundary. To use this range, the development board must have 32GB memory. Do not use this high-address reservation directly on non-32GB boards; adjust the TEE-side memory layout first and update the device tree accordingly.

The following device tree snippet shows the required reservation. If the board device tree already has a `reserved-memory` node, merge these child nodes into the existing node:

```dts
reserved-memory {
    #address-cells = <2>;
    #size-cells = <2>;
    ranges;

    tee_llm_low0: tee-llm-low0@2e00000 {
        reg = <0x0 0x02e00000 0x0 0x05200000>;
        no-map;
    };

    tee_llm_low1: tee-llm-low1@20000000 {
        reg = <0x0 0x20000000 0x0 0x30000000>;
        no-map;
    };

    tee_llm_low2: tee-llm-low2@60000000 {
        reg = <0x0 0x60000000 0x0 0x60000000>;
        no-map;
    };

    tee_llm_high: tee-llm-high@400000000 {
        reg = <0x4 0x00000000 0x3 0x00000000>;
        no-map;
    };
};
```

These addresses must stay consistent with the TEE-side memory configuration in `kernel/arch/aarch64/plat/rk3588/mm/mmparse.c` and `kernel/include/arch/aarch64/plat/rk3588/machine.h`. If the TEE-side memory layout is changed, update the REE-side reserved-memory regions accordingly. The existing TEEOS/TZDRAM reserved-memory configuration must also be kept.

To disable this feature, set:

```Makefile
CHCORE_LLM=OFF
```

## Related code repositories ##

[tee_os_framework](https://gitcode.com/openharmony/tee_tee_os_framework)
