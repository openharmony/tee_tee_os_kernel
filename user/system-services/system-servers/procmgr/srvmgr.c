/*
 * Copyright (c) 2023 Institute of Parallel And Distributed Systems (IPADS), Shanghai Jiao Tong University (SJTU)
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "chcore/defs.h"
#include "chcore/memory.h"
#include "chcore/type.h"
#include <chcore/ipc.h>
#include <chcore/proc.h>
#include <chcore/syscall.h>
#include <chcore-internal/procmgr_defs.h>
#include <string.h>
#include <malloc.h>

#include "proc_node.h"
#include "procmgr_dbg.h"
#include "srvmgr.h"
#include "libchcoreelf.h"
#include "liblaunch.h"
#include "loader.h"

/*
 * Note: This is not an isolated server. It is still a part of procmgr and
 * resides in the same address space. We just take the code related to server
 * management apart from procmgr.c and put it here. Code related to server
 * management like booting a configurable server or get some info about a server
 * should be put in this file from now on.
 */

static int nr_caps = 0;

/*
 * NOTE: We do not initialize the data structure in procmgr to prevent procmgr
 * from registering client of itself. Otherwise, it will lead to memory leak
 * when procmgr calls printf. If put() do not send IPC msg, it can be optimized.
 */
cap_t __procmgr_server_cap;

static pthread_mutex_t read_elf_lock;

/* Array of the caps of the system servers */
static cap_t sys_servers[CONFIG_SERVER_MAX];
/* To launch system server atomically */
static pthread_mutex_t sys_server_locks[CONFIG_SERVER_MAX];

/* For booting system servers. */
static int boot_server(char *srv_name, char *srv_path, cap_t *srv_cap_p,
                       int proc_type)
{
    char *input_argv[PROC_REQ_ARGC_MAX];
    struct proc_node *proc_node;
    input_argv[0] = srv_path;
    proc_node = procmgr_launch_process(
        1, input_argv, srv_name, false, 0, NULL, proc_type);
    if (proc_node == NULL) {
        return -1;
    }
    *srv_cap_p = proc_node->proc_mt_cap;
    return 0;
}

void set_tmpfs_cap(cap_t cap)
{
    sys_servers[SERVER_TMPFS] = cap;
}

void set_chanmgr_cap(cap_t cap)
{
    sys_servers[SERVER_CHANMGR] = cap;
}

/* Return proc_node of new process */
static struct proc_node *
do_launch_process(int argc, char **argv, char *name, bool if_has_parent,
                  badge_t parent_badge, struct user_elf *user_elf,
                  struct loader *loader, struct new_process_args *np_args,
                  int proc_type)
{
    struct proc_node *parent_proc_node;
    struct proc_node *proc_node;
    cap_t caps[3];
    int ret;
    cap_t new_proc_cap;
    cap_t new_proc_mt_cap;
    struct launch_process_args lp_args;

    /* Get parent_proc_node */
    if (if_has_parent) {
        parent_proc_node = get_proc_node(parent_badge);
        assert(parent_proc_node);
    } else {
        parent_proc_node = NULL;
    }

    debug("client: %p, cmd: %s\n", parent_proc_node, argv[1]);
    debug("fsm ipc_struct conn_cap=%d server_type=%d\n",
          fsm_ipc_struct->conn_cap,
          fsm_ipc_struct->server_id);
    printf("[procmgr] Launching %s...\n", name);

    /* Init caps */
    caps[0] = __procmgr_server_cap;
    caps[1] = fsm_server_cap;
    caps[2] = lwip_server_cap;

    for (; nr_caps < 3; nr_caps++) {
        if (caps[nr_caps] == 0)
            break;
    }

    /* Create proc_node to get pid */
    proc_node = new_proc_node(parent_proc_node, name, proc_type);
    assert(proc_node);

    if (np_args == NULL) {
        lp_args.stack_size = MAIN_THREAD_STACK_SIZE;
        lp_args.envp_argc = 0;
        lp_args.envp_argv = NULL;
#ifdef CHCORE_OH_TEE
        lp_args.puuid = NULL;
        lp_args.heap_size = (unsigned long)-1;
#endif /* CHCORE_OH_TEE */
    } else {
        lp_args.stack_size = np_args->stack_size;
        lp_args.envp_argc = np_args->envp_argc;
        lp_args.envp_argv = np_args->envp_argv;
#ifdef CHCORE_OH_TEE
        lp_args.puuid = &np_args->puuid;
        lp_args.heap_size = np_args->heap_size;
#endif /* CHCORE_OH_TEE */
    }
    /* Init launch_process_args */
    lp_args.user_elf = user_elf;
    lp_args.child_process_cap = &new_proc_cap;
    lp_args.child_main_thread_cap = &new_proc_mt_cap;
    lp_args.pmo_map_reqs = NULL;
    lp_args.nr_pmo_map_reqs = 0;
    lp_args.caps = caps;
    lp_args.nr_caps = nr_caps;
    lp_args.cpuid = 0;
    lp_args.argc = argc;
    lp_args.argv = argv;
    lp_args.badge = proc_node->badge;
    lp_args.pid = proc_node->pid;
    lp_args.pcid = proc_node->pcid;
    lp_args.process_name = argv[0];
    if (!strcmp(argv[0], "/tarunner.elf")) {
        lp_args.process_name = name;
    }
    lp_args.load_offset = 0;

#ifdef CHCORE_OH_TEE
    if (lp_args.puuid != NULL) {
        memcpy(&proc_node->puuid, lp_args.puuid, sizeof(spawn_uuid_t));
    } else {
        memset(&proc_node->puuid, 0, sizeof(spawn_uuid_t));
    }
#endif /* CHCORE_OH_TEE */
    proc_node->stack_size = lp_args.stack_size;

    /* Launch process */
    if (loader) {
        ret = launch_process_using_loader(loader, &lp_args);
    } else {
        ret = launch_process_with_pmos_caps(&lp_args);
    }

    if (ret != 0) {
        error("launch process failed\n");
        return NULL;
    }

    /* Set proc_node properties */
    proc_node->state = PROC_STATE_RUNNING;
    proc_node->proc_cap = new_proc_cap;
    proc_node->proc_mt_cap = new_proc_mt_cap;

    debug("new_proc_node: pid=%d cmd=%s parent_pid=%d\n",
          proc_node->pid,
          proc_node->name,
          proc_node->parent ? proc_node->parent->pid : -1);

    return proc_node;
}

struct proc_node *procmgr_launch_process(int argc, char **argv, char *name,
                                         bool if_has_parent,
                                         badge_t parent_badge,
                                         struct new_process_args *np_args,
                                         int proc_type)
{
    int ret;
    struct elf_header *elf_header;
    struct user_elf *user_elf = NULL;
    struct proc_node *node = NULL;
    struct loader *loader = NULL;

    /**
     * Read ELF header first to detect dynamically linked programs.
     */
    ret = load_elf_header_from_fs(argv[0], &elf_header);
    if (ret < 0) {
        error("load_elf_header_from_fs failed, argv[0]=%s\n", argv[0]);
        goto out;
    }

    /**
     * For dynamically linked programs, launch it using CHCORE_LOADER,
     * otherwise load remaining ELF content and launch it directly.
     */
    if (elf_header->e_type == ET_DYN) {
        ret = find_loader(CHCORE_LOADER, &loader);
    } else {
        ret = load_elf_by_header_from_fs(argv[0], elf_header, &user_elf);
    }

    if (ret < 0) {
        error("Error when launching: %s, ret = %d\n", argv[0], ret);
        goto out_free;
    }

    node = do_launch_process(argc,
                             argv,
                             name,
                             if_has_parent,
                             parent_badge,
                             user_elf,
                             loader,
                             np_args,
                             proc_type);
out_free:
    if (user_elf) {
        free_user_elf(user_elf);
    }
    free(elf_header);
out:
    return node;
}

struct proc_node *procmgr_launch_basic_server(int input_argc, char **input_argv,
                                              char *name, bool if_has_parent,
                                              badge_t parent_badge)
{
    int ret;
    struct proc_node *node;
    struct user_elf *user_elf;

    if (strcmp(input_argv[0], "/tmpfs.srv") == 0) {
        pthread_mutex_lock(&read_elf_lock);
        ret = load_elf_from_mem(&__binary_tmpfs_elf_start, &user_elf);
        if (ret == 0) {
            strncpy(user_elf->path, "/tmpfs.srv", ELF_PATH_LEN);
        }
        pthread_mutex_unlock(&read_elf_lock);
    } else if (strcmp(input_argv[0], "/fsm.srv") == 0) {
        pthread_mutex_lock(&read_elf_lock);
        ret = load_elf_from_mem(&__binary_fsm_elf_start, &user_elf);
        if (ret == 0) {
            strncpy(user_elf->path, "/fsm.srv", ELF_PATH_LEN);
        }
        pthread_mutex_unlock(&read_elf_lock);
    } else {
        ret = -1;
    }
    if (ret < 0) {
        debug("Error args or parsing elf error!\n");
        return NULL;
    }
    node = do_launch_process(input_argc,
                             input_argv,
                             name,
                             if_has_parent,
                             parent_badge,
                             user_elf,
                             NULL,
                             NULL,
                             SYSTEM_SERVER);
    free_user_elf(user_elf);
    return node;
}

void handle_get_server_cap(ipc_msg_t *ipc_msg, struct proc_request *pr)
{
    cap_t server_cap;
    int ret = 0;

    /* Check if server_id is valid */
    if (pr->get_server_cap.server_id < 0
        || pr->get_server_cap.server_id >= CONFIG_SERVER_MAX) {
        ipc_return(ipc_msg, -EINVAL);
    }

    pthread_mutex_lock(&sys_server_locks[pr->get_server_cap.server_id]);

    server_cap = sys_servers[pr->get_server_cap.server_id];

    if (server_cap != -1) {
        /* Server already booted */
        ipc_msg->cap_slot_number = 1;
        ipc_set_msg_cap(ipc_msg, 0, server_cap);
        goto out;
    } else {
        /* Server not booted */
        switch (pr->get_server_cap.server_id) {
        case SERVER_TMPFS:
            printf("Tmpfs does not start up now.\n");
            BUG_ON(1);
            goto out;
        case SERVER_CHANMGR:
#ifdef CHCORE_OH_TEE
            ret = boot_server("chanmgr",
                              "/chanmgr.srv",
                              sys_servers + SERVER_CHANMGR,
                              SYSTEM_SERVER);
            if (ret == -1) {
                goto out;
            }
            ipc_msg->cap_slot_number = 1;
            ipc_set_msg_cap(ipc_msg, 0, sys_servers[SERVER_CHANMGR]);
#else
            error("chanmgr NOT enabled!\n");
            ret = -1;
#endif /* CHCORE_OH_TEE */
            goto out;
        default:
            error("unvalid server id: %x\n", pr->get_server_cap.server_id);
            goto out;
        }
    }

out:
    pthread_mutex_unlock(&sys_server_locks[pr->get_server_cap.server_id]);
    if (ret == 0) {
        ipc_return_with_cap(ipc_msg, 0);
    } else {
        ipc_return(ipc_msg, ret);
    }
}

void boot_secondary_servers(void)
{
}

void init_srvmgr(void)
{
    /* Init read_elf_lock */
    pthread_mutex_init(&read_elf_lock, NULL);
    /* Init server array */
    for (int i = 0; i < CONFIG_SERVER_MAX; ++i) {
        sys_servers[i] = -1;
        pthread_mutex_init(&sys_server_locks[i], NULL);
    }
}
