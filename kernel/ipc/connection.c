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
/*
 * Inter-**Process** Communication.
 *
 * connection: between a client cap_group and a server cap_group (two processes)
 * We store connection cap in a process' cap_group, so each thread in it can
 * use that connection.
 *
 * A connection (cap) can be used by any thread in the client cap_group.
 * A connection will be **only** served by one server thread while
 * one server thread may serve multiple connections.
 *
 * There is one PMO_SHM binding with one connection.
 *
 * Connection can only serve one IPC request at the same time.
 * Both user and kernel should check the "busy_state" of a connection.
 * Besides, register thread can also serve one registration request for one
 * time.
 *
 * Since a connection can only be shared by client threads in the same process,
 * a connection has only-one **badge** to identify the process.
 * During ipc_call, the kernel can set **badge** as an argument in register.
 *
 * Overview:
 * **IPC registration (control path)**
 *  - A server thread (S1) invokes **sys_register_server** with
 *    a register_cb_thread (S2)
 *
 *  - A client thread (C) invokes **sys_register_client(S1)**
 *	- invokes (switches) to S2 actually
 *  - S2 invokes **sys_ipc_register_cb_return** with a handler_thread (S3)
 *	- S3 will serve IPC requests later
 *	- switches back to C (finish the IPC registration)
 *
 * **IPC call/reply (data path)**
 *  - C invokes **sys_ipc_call**
 *	- switches to S3
 *  - S3 invokes **sys_ipc_return**
 *	- switches to C
 */

#include <common/errno.h>
#include <ipc/connection.h>
#include <irq/irq.h>
#include <mm/kmalloc.h>
#include <mm/uaccess.h>
#include <object/memory.h>
#include <sched/context.h>

/*
 * @brief A legal ipc_msg should be placed in shared memory of a IPC connection.
 * Besides, ipc_msg is actually a header of following custom data and cap array,
 * The latter ones should also be placed in shared memory. This function checks
 * above constraints, only return 0 if all of them are satisfied. Otherwise -1
 * is returned to indicate invalid or malicious ipc_msg.
 *
 * @param user_ipc_msg_ptr: pointer to ipc_msg passed by user program
 * @param shm_start: start address of shared memory of a connection. This
 * address may be server-side or client-side. When checking ipc_msg for
 * client-side, client- side start address should be passed in, and vice versa.
 * @param shm_size: size of shared memory
 * @return int: 0 indicates a legal ipc_msg, otherwise -1 is to indicate invalid
 * or malicious ipc_msg.
 */
static int check_ipc_msg_in_shm(struct ipc_msg *user_ipc_msg_ptr,
                                vaddr_t shm_start, size_t shm_size)
{
    int ret;
    struct ipc_msg ipc_msg_kbuf;
    char *shm_start_ptr = (char *)shm_start;
    char *shm_end = shm_start_ptr + shm_size;
    char *ipc_msg_start = (char *)user_ipc_msg_ptr;
    char *ipc_msg_end = ipc_msg_start + sizeof(struct ipc_msg);
    char *ipc_data_start, *ipc_data_end;
    char *ipc_caps_start, *ipc_caps_end;

    /**
     * checks of ipc_data and ipc_caps depends on data in ipc_msg, so
     * we must check ipc_msg itself is legal first.
     */
    if (!(shm_start_ptr <= ipc_msg_start && ipc_msg_end <= shm_end)) {
        return -1;
    }

    ret = copy_from_user(&ipc_msg_kbuf, ipc_msg_start, sizeof(struct ipc_msg));

    if (ret < 0) {
        return ret;
    }

    ipc_data_start = ipc_msg_start + ipc_msg_kbuf.data_offset;
    ipc_data_end = ipc_data_start + ipc_msg_kbuf.data_len;

    ipc_caps_start = ipc_msg_start + ipc_msg_kbuf.cap_slots_offset;
    ipc_caps_end =
        ipc_caps_start + ipc_msg_kbuf.cap_slot_number * sizeof(cap_t);

    if (!(shm_start_ptr <= ipc_data_start && ipc_data_end <= shm_end)) {
        return -1;
    }

    if (!(shm_start_ptr <= ipc_caps_start && ipc_caps_end <= shm_end)) {
        return -1;
    }

    return 0;
}

/*
 * Overall, a server thread that declares a serivce with this interface
 * should specify:
 * @ipc_routine (the real ipc service routine entry),
 * @register_thread_cap (another server thread for handling client
 * registration), and
 * @destructor (one routine invoked when some connnection is closed).
 */
static int register_server(struct thread *server, unsigned long ipc_routine,
                           cap_t register_thread_cap, unsigned long destructor)
{
    struct ipc_server_config *config;
    struct thread *register_cb_thread;
    struct ipc_server_register_cb_config *register_cb_config;

    BUG_ON(server == NULL);
    if (server->general_ipc_config != NULL) {
        kdebug("A server thread can only invoke **register_server** once!\n");
        return -EINVAL;
    }

    /*
     * Check the passive thread in server for handling
     * client registration.
     */
    register_cb_thread =
        obj_get(current_cap_group, register_thread_cap, TYPE_THREAD);
    if (!register_cb_thread) {
        kdebug("A register_cb_thread is required.\n");
        return -ECAPBILITY;
    }

    if (register_cb_thread->thread_ctx->type != TYPE_REGISTER) {
        kdebug("The register_cb_thread should be TYPE_REGISTER!\n");
        obj_put(register_cb_thread);
        return -EINVAL;
    }

    config = kmalloc(sizeof(*config));

    /*
     * @ipc_routine will be the real ipc_routine_entry.
     * No need to validate such address because the server just
     * kill itself if the address is illegal.
     */
    config->declared_ipc_routine_entry = ipc_routine;

    /* Record the registration cb thread */
    config->register_cb_thread = register_cb_thread;

    register_cb_config = kmalloc(sizeof(*register_cb_config));
    register_cb_thread->general_ipc_config = register_cb_config;

    /*
     * This lock will be used to prevent concurrent client threads
     * from registering.
     * In other words, a register_cb_thread can only serve
     * registration requests one-by-one.
     */
    lock_init(&register_cb_config->register_lock);

    /* Record PC as well as the thread's initial stack (SP). */
    register_cb_config->register_cb_entry =
        arch_get_thread_next_ip(register_cb_thread);
    register_cb_config->register_cb_stack =
        arch_get_thread_stack(register_cb_thread);
    register_cb_config->destructor = destructor;
    obj_put(register_cb_thread);

#if defined(CHCORE_ARCH_AARCH64)
    /* The following fence can ensure: the config related data,
     * e.g., the register_lock, can been seen when
     * server->general_ipc_config is set.
     */
    smp_mb();
#else
    /* TSO: the fence is not required. */
#endif

    /*
     * The last step: fill the general_ipc_config.
     * This field is also treated as the whether the server thread
     * declares an IPC service (or makes the service ready).
     */
    server->general_ipc_config = config;

    return 0;
}

void connection_deinit(void *conn)
{
    /* For now, no de-initialization is required */
}

/* Just used for storing the results of function create_connection */
struct client_connection_result {
    cap_t client_conn_cap;
    cap_t server_conn_cap;
    cap_t server_shm_cap;
    struct ipc_connection *conn;
};

static int get_pmo_size(cap_t pmo_cap)
{
    struct pmobject *pmo;
    int size;

    pmo = obj_get(current_cap_group, pmo_cap, TYPE_PMO);
    BUG_ON(!pmo);

    size = pmo->size;
    obj_put(pmo);

    return size;
}

/*
 * The function will create an IPC connection and initialize the client side
 * information. (used in sys_register_client)
 *
 * The server (register_cb_thread) will initialize the server side information
 * later (in sys_ipc_register_cb_return).
 */
static int create_connection(struct thread *client, struct thread *server,
                             int shm_cap_client, unsigned long shm_addr_client,
                             struct client_connection_result *res)
{
    cap_t shm_cap_server;
    struct ipc_connection *conn;
    int ret = 0;
    cap_t conn_cap = 0, server_conn_cap = 0;

    BUG_ON((client == NULL) || (server == NULL));

    /*
     * Copy the shm_cap to the server.
     *
     * It is reasonable to count the shared memory usage on the client.
     * So, a client should prepare the shm and tell the server.
     */
    shm_cap_server =
        cap_copy(current_cap_group, server->cap_group, shm_cap_client);

    /* Create struct ipc_connection */
    conn = obj_alloc(TYPE_CONNECTION, sizeof(*conn));
    if (!conn) {
        ret = -ENOMEM;
        goto out_fail;
    }

    /* Initialize the connection (begin).
     *
     * Note that now client is applying to build the connection
     * instead of issuing an IPC.
     */
    conn->is_valid = OBJECT_STATE_INVALID;
    conn->current_client_thread = client;
    /*
     * The register_cb_thread in server will assign the
     * server_handler_thread later.
     */
    conn->server_handler_thread = NULL;
    /*
     * The badge is now generated by the process who creates the client
     * thread. Usually, the process is the procmgr user-space service.
     * The badge needs to be unique.
     *
     * Before a process exits, it needs to close the connection with
     * servers. Otherwise, a later process may pretend to be it
     * because the badge is based on PID (if a PID is reused,
     * the same badge occur).
     * Or, the kernel should notify the server to close the
     * connections when some client exits.
     */
    conn->client_badge = current_cap_group->badge;
    conn->shm.client_shm_uaddr = shm_addr_client;
    conn->shm.shm_size = get_pmo_size(shm_cap_client);
    conn->shm.shm_cap_in_client = shm_cap_client;
    conn->shm.shm_cap_in_server = shm_cap_server;
    lock_init(&conn->ownership);
    /* Initialize the connection (end) */

    /* After initializing the object,
     * give the ipc_connection (cap) to the client.
     */
    conn_cap = cap_alloc(current_cap_group, conn);
    if (conn_cap < 0) {
        ret = conn_cap;
        goto out_free_obj;
    }

    /* Give the ipc_connection (cap) to the server */
    server_conn_cap = cap_copy(current_cap_group, server->cap_group, conn_cap);
    if (server_conn_cap < 0) {
        ret = server_conn_cap;
        goto out_free_cap;
    }

    /* Preapre the return results */
    res->client_conn_cap = conn_cap;
    res->server_conn_cap = server_conn_cap;
    res->server_shm_cap = shm_cap_server;
    res->conn = conn;

    return 0;

out_free_cap:
    cap_free(current_cap_group, conn_cap);
    conn = NULL;
out_free_obj:
    obj_free(conn);
out_fail:
    return ret;
}

/*
 * Grap the ipc lock before doing any modifications including
 * modifing the conn or sending the caps.
 */
static inline int grab_ipc_lock(struct ipc_connection *conn)
{
    struct thread *target;
    struct ipc_server_handler_config *handler_config;

    target = conn->server_handler_thread;
    handler_config =
        (struct ipc_server_handler_config *)target->general_ipc_config;

    /*
     * Grabing the ipc_lock can ensure:
     * First, avoid invoking the same handler thread.
     * Second, also avoid using the same connection.
     *
     * perf in Qemu: lock & unlock (without contention) just takes
     * about 20 cycles on x86_64.
     */

    /* Use try-lock, otherwise deadlock may happen
     * deadlock: T1: ipc-call -> Server -> resched to T2: ipc-call
     *
     * Although lock is added in user-ipc-lib, a buggy app may dos
     * the kernel.
     */

    if (try_lock(&handler_config->ipc_lock) != 0)
        return -EIPCRETRY;

    return 0;
}

static inline int release_ipc_lock(struct ipc_connection *conn)
{
    struct thread *target;
    struct ipc_server_handler_config *handler_config;

    target = conn->server_handler_thread;
    handler_config =
        (struct ipc_server_handler_config *)target->general_ipc_config;

    unlock(&handler_config->ipc_lock);

    return 0;
}

static void thread_migrate_to_server(struct ipc_connection *conn,
                                     unsigned long arg)
{
    struct thread *target;
    struct ipc_server_handler_config *handler_config;

    target = conn->server_handler_thread;
    handler_config =
        (struct ipc_server_handler_config *)target->general_ipc_config;

    /*
     * Note that a server ipc handler thread can be assigned to multiple
     * connections.
     * So, it is necessary to record which connection is active.
     */
    handler_config->active_conn = conn;

    /*
     * Note that multiple client threads may share a same connection.
     * So, it is necessary to record which client thread is active.
     * Then, the server can transfer the control back to it after finishing
     * the IPC.
     */
    conn->current_client_thread = current_thread;

    /* Mark current_thread as TS_WAITING */
    current_thread->thread_ctx->state = TS_WAITING;

    /* Pass the scheduling context */
    target->thread_ctx->sc = current_thread->thread_ctx->sc;

    /* Set the target thread SP/IP/arguments */
    arch_set_thread_stack(target, handler_config->ipc_routine_stack);
    arch_set_thread_next_ip(target, handler_config->ipc_routine_entry);
    /* First argument: ipc_msg */
    arch_set_thread_arg0(target, arg);
    /* Second argument: client_badge */
    arch_set_thread_arg1(target, conn->client_badge);
#ifdef CHCORE_OH_TEE
    /* Third argument: pid */
    arch_set_thread_arg2(target, conn->current_client_thread->cap_group->pid);
    /* Fourth argument: tid */
    arch_set_thread_arg3(target, conn->current_client_thread->cap);
#endif /* CHCORE_OH_TEE */
    set_thread_arch_spec_state_ipc(target);

    /* Switch to the target thread */
    sched_to_thread(target);

    /* Function never return */
    BUG_ON(1);
}

static void thread_migrate_to_client(struct thread *client,
                                     unsigned long ret_value)
{
    /* Set return value for the target thread */
    arch_set_thread_return(client, ret_value);

    /* Switch to the client thread */
    sched_to_thread(client);

    /* Function never return */
    BUG_ON(1);
}

struct client_shm_config {
    cap_t shm_cap;
    unsigned long shm_addr;
};

/* IPC related system calls */

int sys_register_server(unsigned long ipc_routine, cap_t register_thread_cap,
                        unsigned long destructor)
{
    return register_server(
        current_thread, ipc_routine, register_thread_cap, destructor);
}

cap_t sys_register_client(cap_t server_cap, unsigned long shm_config_ptr)
{
    struct thread *client;
    struct thread *server;

    /*
     * No need to initialize actually.
     * However, fbinfer will complain without zeroing because
     * it cannot tell copy_from_user.
     */
    struct client_shm_config shm_config = {0};
    int r;
    struct client_connection_result res;

    struct ipc_server_config *server_config;
    struct thread *register_cb_thread;
    struct ipc_server_register_cb_config *register_cb_config;

    client = current_thread;

    server = obj_get(current_cap_group, server_cap, TYPE_THREAD);
    if (!server) {
        r = -ECAPBILITY;
        goto out_fail;
    }

    server_config = (struct ipc_server_config *)(server->general_ipc_config);
    if (!server_config) {
        r = -EIPCRETRY;
        goto out_fail;
    }

    /*
     * Locate the register_cb_thread first.
     * And later, directly transfer the control flow to it
     * for finishing the registration.
     *
     * The whole registration procedure:
     * client thread -> server register_cb_thread -> client threrad
     */
    register_cb_thread = server_config->register_cb_thread;
    register_cb_config = (struct ipc_server_register_cb_config
                              *)(register_cb_thread->general_ipc_config);

    /* Acquiring register_lock: avoid concurrent client registration.
     *
     * Use try_lock instead of lock since the unlock operation is done by
     * another thread and ChCore does not support mutex.
     * Otherwise, dead lock may happen.
     */
    if (try_lock(&register_cb_config->register_lock) != 0) {
        r = -EIPCRETRY;
        goto out_fail;
    }

    /* Validate the user addresses before accessing them */
    if (check_user_addr_range(shm_config_ptr, sizeof(shm_config) != 0)) {
        r = -EINVAL;
        goto out_fail_unlock;
    }

    copy_from_user(
        (void *)&shm_config, (void *)shm_config_ptr, sizeof(shm_config));

    /* Map the pmo of the shared memory */
    r = map_pmo_in_current_cap_group(
        shm_config.shm_cap, shm_config.shm_addr, VMR_READ | VMR_WRITE);

    if (r != 0) {
        goto out_fail_unlock;
    }

    /* Create the ipc_connection object */
    r = create_connection(
        client, server, shm_config.shm_cap, shm_config.shm_addr, &res);

    if (r != 0) {
        goto out_fail_unlock;
    }

    /* Record the connection cap of the client process */
    register_cb_config->conn_cap_in_client = res.client_conn_cap;
    register_cb_config->conn_cap_in_server = res.server_conn_cap;
    /* Record the server_shm_cap for current connection */
    register_cb_config->shm_cap_in_server = res.server_shm_cap;

    /* Mark current_thread as TS_WAITING */
    current_thread->thread_ctx->state = TS_WAITING;

    /* Set target thread SP/IP/arg */
    arch_set_thread_stack(register_cb_thread,
                          register_cb_config->register_cb_stack);
    arch_set_thread_next_ip(register_cb_thread,
                            register_cb_config->register_cb_entry);
    arch_set_thread_arg0(register_cb_thread,
                         server_config->declared_ipc_routine_entry);
    obj_put(server);

    /* Pass the scheduling context */
    register_cb_thread->thread_ctx->sc = current_thread->thread_ctx->sc;

    /* On success: switch to the cb_thread of server  */
    sched_to_thread(register_cb_thread);

    /* Never return */
    BUG_ON(1);

out_fail_unlock:
    unlock(&register_cb_config->register_lock);
out_fail: /* Maybe EAGAIN */
    if (server)
        obj_put(server);
    return r;
}

#define MAX_CAP_TRANSFER 16
static int ipc_send_cap(struct cap_group *target_cap_group,
                        struct ipc_msg *ipc_msg, unsigned int cap_num)
{
    int i, r;
    unsigned int cap_slots_offset;
    cap_t *cap_buf;
    vaddr_t uaddr;

    if (cap_num >= MAX_CAP_TRANSFER) {
        r = -EINVAL;
        goto out_fail;
    }

    uaddr = (vaddr_t)&ipc_msg->cap_slots_offset;
    if (check_user_addr_range(uaddr, sizeof(cap_slots_offset)) != 0) {
        r = -EINVAL;
        goto out_fail;
    }

    r = copy_from_user(
        &cap_slots_offset, (void *)uaddr, sizeof(cap_slots_offset));
    if (r)
        goto out_fail;

    uaddr = (vaddr_t)((char *)ipc_msg + cap_slots_offset);
    if (check_user_addr_range(uaddr, sizeof(*cap_buf) * cap_num) != 0) {
        r = -EINVAL;
        goto out_fail;
    }

    cap_buf = kmalloc(cap_num * sizeof(*cap_buf));
    if (!cap_buf) {
        r = -ENOMEM;
        goto out_fail;
    }

    r = copy_from_user(cap_buf, (void *)uaddr, sizeof(*cap_buf) * cap_num);
    if (r) {
        i = 0;
        goto out_free_cap;
    }

    for (i = 0; i < cap_num; i++) {
        cap_t dest_cap;

        dest_cap = cap_copy(current_cap_group, target_cap_group, cap_buf[i]);
        if (dest_cap < 0) {
            r = dest_cap;
            goto out_free_cap;
        }

        cap_buf[i] = dest_cap;
    }

    
    r = copy_to_user((void *)uaddr, cap_buf, sizeof(*cap_buf) * cap_num);
    if (r)
        goto out_free_cap;

    kfree(cap_buf);
    return 0;

out_free_cap:
    for (--i; i >= 0; i--)
        cap_free(target_cap_group, cap_buf[i]);
    kfree(cap_buf);
out_fail:
    return r;
}

/* Issue an IPC request */
unsigned long sys_ipc_call(cap_t conn_cap, struct ipc_msg *ipc_msg_in_client,
                           unsigned int cap_num)
{
    struct ipc_connection *conn;
    vaddr_t ipc_msg_in_server = 0;
    int r = 0;

    if (!ipc_msg_in_client) {
        return -EINVAL;
    }

    conn = obj_get(current_cap_group, conn_cap, TYPE_CONNECTION);
    if (unlikely(!conn)) {
        return -ECAPBILITY;
    }

    if (try_lock(&conn->ownership) == 0) {
        /*
         * Succeed in locking.
         *
         * If the connection is INVALID (setted in sys_ipc_return or
         * sys_recycle_cap_group),
         * then returns an ERROR to the invoker.
         */
        if (conn->is_valid == OBJECT_STATE_INVALID) {
            unlock(&conn->ownership);
            obj_put(conn);
            return -EINVAL;
        }
    } else {
        /* Fails to lock the connection */
        obj_put(conn);

        if (current_thread->thread_ctx->thread_exit_state == TE_EXITING) {
            /* The connection is locked by the recycler */

            if (current_thread->thread_ctx->type == TYPE_SHADOW) {
                /*
                 * The current thread is B in chained IPC
                 * (A:B:C). B will receives an Error.
                 * We hope B invokes sys_ipc_return to give
                 * the control flow back to A and unlock the
                 * related connection.
                 */
                return -ESRCH;
            } else {
                /* Current thread will be set to exited by
                 * the scheduler */
                sched();
                eret_to_thread(switch_context());
            }
        } else {
            /* The connection is locked by someone else */
            return -EIPCRETRY;
        }
    }

    /*
     * try_lock may fail and returns egain.
     * No modifications happen before locking, so the client
     * can simply try again later.
     */
    if ((r = grab_ipc_lock(conn)) != 0)
        goto out_obj_put;

    if (ipc_msg_in_client != NULL) {
        /* ipc_msg should be placed in IPC shared memory */
        if (check_ipc_msg_in_shm(ipc_msg_in_client,
                                 conn->shm.client_shm_uaddr,
                                 conn->shm.shm_size)
            < 0) {
            r = -EINVAL;
            goto out_release_lock;
        }

        if (cap_num != 0) {
            r = ipc_send_cap(conn->server_handler_thread->cap_group,
                             ipc_msg_in_client,
                             cap_num);
            if (r < 0)
                goto out_release_lock;
        }

        conn->user_ipc_msg = ipc_msg_in_client;

        /*
         * A shm is bound to one connection.
         * But, the client and server can map the shm at different addresses.
         * So, we re-calculate the ipc_msg (in the shm) address here.
         */
        ipc_msg_in_server = (vaddr_t)ipc_msg_in_client
                            - conn->shm.client_shm_uaddr
                            + conn->shm.server_shm_uaddr;
    }

    /* Call server (handler thread) */
    thread_migrate_to_server(conn, ipc_msg_in_server);

    BUG("should not reach here\n");

out_release_lock:
    release_ipc_lock(conn);
out_obj_put:
    unlock(&conn->ownership);
    obj_put(conn);
    return r;
}

int sys_ipc_return(unsigned long ret, unsigned int cap_num)
{
    struct ipc_server_handler_config *handler_config;
    struct ipc_connection *conn;
    struct thread *client;

    /* Get the currently active connection */
    handler_config =
        (struct ipc_server_handler_config *)current_thread->general_ipc_config;
    conn = handler_config->active_conn;

    if (!conn)
        return -EINVAL;

    /*
     * Get the client thread that issues this IPC.
     *
     * Note that it is **unnecessary** to set the field to NULL
     * i.e., conn->current_client_thread = NULL.
     */
    client = conn->current_client_thread;

    /* Step-1. check if current_thread (conn->server_handler_thread) is
     * TE_EXITING
     *     -> Yes: set server_handler_thread to NULL, then continue to
     * Step-2
     *     -> No: continue to Step-2
     */
    if (current_thread->thread_ctx->thread_exit_state == TE_EXITING) {
        kdebug("%s:%d Step-1\n", __func__, __LINE__);

        conn->is_valid = OBJECT_STATE_INVALID;

        current_thread->thread_ctx->thread_exit_state = TE_EXITED;
        current_thread->thread_ctx->state = TS_EXIT;

        /* Returns an error to the client */
        ret = -ESRCH;
    }

    /* Step-2. check if client_thread is TS_EXITING
     *     -> Yes: set current_client_thread to NULL
     *	  Then check if client is shadow
     *         -> No: set client to TS_EXIT and then sched
     *         -> Yes: return to client (it will recycle itself at next ipc_return)
     *     -> No: return normally
     */
    if (client->thread_ctx->thread_exit_state == TE_EXITING) {
        kdebug("%s:%d Step-2\n", __func__, __LINE__);

        /*
         * Currently, a connection is assumed to belong to the client
         * process. So, it the client is exiting, then the connection is
         * useless.
         */

        conn->is_valid = OBJECT_STATE_INVALID;

        /* If client thread is not TYPE_SHADOW, then directly mark it as
         * exited and reschedule.
         *
         * Otherwise, client thread is B in a chained IPC (A:B:C) and
         * current_thread is C. So, C returns to B and later B will
         * returns to A.
         */
        if (client->thread_ctx->type != TYPE_SHADOW) {
            kdebug("%s:%d Step-2.0\n", __func__, __LINE__);
            handler_config->active_conn = NULL;

            current_thread->thread_ctx->state = TS_WAITING;

            current_thread->thread_ctx->sc = NULL;

            unlock(&handler_config->ipc_lock);

            unlock(&conn->ownership);
            obj_put(conn);

            client->thread_ctx->thread_exit_state = TE_EXITED;
            client->thread_ctx->state = TS_EXIT;

            sched();
            eret_to_thread(switch_context());
            /* The control flow will never go through */
        }
    }

    if (cap_num != 0) {
        struct ipc_msg *server_ipc_msg;

        if (unlikely(conn->user_ipc_msg == NULL))
            return -EINVAL;

        /**
         * sanity of conn->user_ipc_msg has been checked during sys_ipc_call
         */
        server_ipc_msg = (struct ipc_msg *)((unsigned long)conn->user_ipc_msg
                                            - conn->shm.client_shm_uaddr
                                            + conn->shm.server_shm_uaddr);

        int r = ipc_send_cap(
            conn->current_client_thread->cap_group, server_ipc_msg, cap_num);
        if (r < 0)
            return r;
    }

    /* Set active_conn to NULL since the IPC will finish sooner */
    handler_config->active_conn = NULL;

    /*
     * Return control flow (sched-context) back later.
     * Set current_thread state to TS_WAITING again.
     */
    current_thread->thread_ctx->state = TS_WAITING;

    /*
     * Shadow thread should not any more use
     * the client's scheduling context.
     *
     * Note that the sc of server_thread (current_thread) must be set to
     * NULL (named OP-SET-NULL) **before** unlocking the lock.
     * Otherwise, a following client thread may transfer its sc to the
     * server_thread before OP-SET-NULL.
     */

    current_thread->thread_ctx->sc = NULL;

    /*
     * Release the ipc_lock to mark the server_handler_thread can
     * serve other requests now.
     */
    unlock(&handler_config->ipc_lock);

    unlock(&conn->ownership);
    obj_put(conn);

    /* Return to client */
    thread_migrate_to_client(client, ret);
    BUG("should not reach here\n");
}

int sys_ipc_register_cb_return(cap_t server_handler_thread_cap,
                               unsigned long server_thread_exit_routine,
                               unsigned long server_shm_addr)
{
    struct ipc_server_register_cb_config *config;
    struct ipc_connection *conn;
    struct thread *client_thread;

    struct thread *ipc_server_handler_thread;
    struct ipc_server_handler_config *handler_config;
    int r = -ECAPBILITY;

    config = (struct ipc_server_register_cb_config *)
                 current_thread->general_ipc_config;

    if (!config)
        goto out_fail;

    conn =
        obj_get(current_cap_group, config->conn_cap_in_server, TYPE_CONNECTION);

    if (!conn)
        goto out_fail;

    /*
     * @server_handler_thread_cap from server.
     * Server uses this handler_thread to serve ipc requests.
     */
    ipc_server_handler_thread = (struct thread *)obj_get(
        current_cap_group, server_handler_thread_cap, TYPE_THREAD);

    if (!ipc_server_handler_thread)
        goto out_fail_put_conn;

    /* Map the shm of the connection in server */
    r = map_pmo_in_current_cap_group(
        config->shm_cap_in_server, server_shm_addr, VMR_READ | VMR_WRITE);
    if (r != 0)
        goto out_fail_put_thread;

    /* Get the client_thread that issues this registration */
    client_thread = conn->current_client_thread;
    /*
     * Set the return value (conn_cap) for the client here
     * because the server has approved the registration.
     */
    arch_set_thread_return(client_thread, config->conn_cap_in_client);

    /*
     * Initialize the ipc configuration for the handler_thread (begin)
     *
     * When the handler_config isn't NULL, it means this server handler
     * thread has been initialized before. If so, skip the initialization.
     * This will happen when a server uses one server handler thread for
     * serving multiple client threads.
     */
    if (!ipc_server_handler_thread->general_ipc_config) {
        handler_config = (struct ipc_server_handler_config *)kmalloc(
            sizeof(*handler_config));
        ipc_server_handler_thread->general_ipc_config = handler_config;
        lock_init(&handler_config->ipc_lock);

        /*
         * Record the initial PC & SP for the handler_thread.
         * For serving each IPC, the handler_thread starts from the
         * same PC and SP.
         */
        handler_config->ipc_routine_entry =
            arch_get_thread_next_ip(ipc_server_handler_thread);
        handler_config->ipc_routine_stack =
            arch_get_thread_stack(ipc_server_handler_thread);
        handler_config->ipc_exit_routine_entry = server_thread_exit_routine;
        handler_config->destructor = config->destructor;
    }
    obj_put(ipc_server_handler_thread);
    /* Initialize the ipc configuration for the handler_thread (end) */

    /* Fill the server information in the IPC connection. */
    conn->shm.server_shm_uaddr = server_shm_addr;
    conn->server_handler_thread = ipc_server_handler_thread;
    conn->is_valid = OBJECT_STATE_VALID;
    conn->current_client_thread = NULL;
    conn->conn_cap_in_client = config->conn_cap_in_client;
    conn->conn_cap_in_server = config->conn_cap_in_server;
    obj_put(conn);

    /*
     * Return control flow (sched-context) back later.
     * Set current_thread state to TS_WAITING again.
     */
    current_thread->thread_ctx->state = TS_WAITING;

    unlock(&config->register_lock);

    /* Register thread should not any more use the client's scheduling
     * context. */
    current_thread->thread_ctx->sc = NULL;

    /* Finish the registration: switch to the original client_thread */
    sched_to_thread(client_thread);
    /* Nerver return */

out_fail_put_thread:
    obj_put(ipc_server_handler_thread);
out_fail_put_conn:
    obj_put(conn);
out_fail:
    return r;
}

void sys_ipc_exit_routine_return(void)
{
    struct ipc_server_handler_config *config;

    config =
        (struct ipc_server_handler_config *)current_thread->general_ipc_config;
    if (!config) {
        goto out;
    }
    /*
     * Set the server handler thread state to TS_WAITING again
     * so that it can be migrated to from the client.
     */
    current_thread->thread_ctx->state = TS_WAITING;
    kfree(current_thread->thread_ctx->sc);
    unlock(&config->ipc_lock);
out:
    sched();
    eret_to_thread(switch_context());
}
