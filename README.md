# tee_tee_os_kernel 仓介绍 #

tee_tee_os_kernel 部件主要包含 TEE 的内核部分，采用微内核架构设计。

### 一、tee_tee_os_kernel 部件模块划分 ###
<table>
<th>子模块名称</th>
<th>模块简介</th>
<tr>
<td> kernel/ipc </td><td> 进程间通信模块 </td>
</tr><tr>
<td> kernel/irq </td><td> 中断处理模块 </td>
</tr><tr>
<td> kernel/mm </td><td> 内存管理模块 </td>
</tr><tr>
<td> kernel/object </td><td> 内核对象管理 </td>
</tr><tr>
<td> kernel/sched </td><td> 线程调度模块 </td>
</tr><tr>
<td> user/chcore-libs/sys-libs/libohtee </td><td> 框架所依赖的库函数 </td>
</tr><tr>
<td> user/system-services/system-servers/procmgr </td><td> 负责进程管理，拥有所有进程的信息 </td>
</tr><tr>
<td> user/system-services/system-servers/fs_base </td><td> 虚拟文件系统模块 </td>
</tr><tr>
<td> user/system-services/system-servers/fsm </td><td> 文件系统管理模块 </td>
</tr><tr>
<td> user/system-services/system-servers/tmpfs </td><td> 内存文件系统模块 </td>
</tr><tr>
<td> user/system-services/system-servers/chanmgr </td><td> 管理 channel 的命名、索引及分发 </td>
</tr>


</table>

### 二、tee_tee_os_kernel 部件代码目录结构 ###
```
base/tee/tee_tee_os_kernel
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