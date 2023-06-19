/*
 * Copyright (c) 2023 Institute of Parallel And Distributed Systems (IPADS)
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <object/recycle.h>
#include <object/cap_group.h>
#include <object/thread.h>
#include <common/list.h>
#include <common/sync.h>
#include <common/util.h>
#include <common/bitops.h>
#include <mm/kmalloc.h>
#include <mm/mm.h>
#include <mm/vmspace.h>
#include <mm/uaccess.h>
#include <lib/printk.h>
#include <ipc/notification.h>
#include <irq/irq.h>
#include <lib/ring_buffer.h>
#include <sched/context.h>
#include <syscall/syscall_hooks.h>
#ifdef CHCORE_OH_TEE
#include <ipc/channel.h>
#endif /* CHCORE_OH_TEE */

struct recycle_msg {
    badge_t badge;
    int exitcode;
    int padding;
};

struct recycle_msg_node {
    struct list_head node;
    struct recycle_msg msg;
};

struct notification *recycle_notification = NULL;
struct ring_buffer *recycle_msg_buffer;
/* This list is only used when the recycle_msg_buffer is full */
struct list_head recycle_msg_head;
struct lock recycle_buffer_lock;

int sys_register_recycle(cap_t notifc_cap, vaddr_t msg_buffer)
{
    int ret;

    if ((ret = hook_sys_register_recycle(notifc_cap, msg_buffer)) != 0)
        return ret;

    recycle_notification =
        obj_get(current_cap_group, notifc_cap, TYPE_NOTIFICATION);

    if (recycle_notification == NULL)
        return -ECAPBILITY;

    ret = trans_uva_to_kva(msg_buffer, (vaddr_t *)&recycle_msg_buffer);
    if (ret != 0)
        return -EINVAL;

    init_list_head(&recycle_msg_head);
    lock_init(&recycle_buffer_lock);

    return 0;
}

/*
 * Kernel uses this function to invoke the recycle thread in procmgr.
 * proc_badge is the badge of the process to recycle.
 */
void notify_user_recycler(badge_t proc_badge, int exitcode)
{
    /* lock the recyle buffer first */
    lock(&recycle_buffer_lock);

    if (if_buffer_full(recycle_msg_buffer)) {
        /* Save the msg in the list for now */
        struct recycle_msg_node *msg;

        msg = kmalloc(sizeof(*msg));
        if (!msg) {
            kwarn("No memory when %s", __func__);
            return;
        }
        msg->msg.badge = proc_badge;
        msg->msg.exitcode = exitcode;
        list_add(&msg->node, &recycle_msg_head);
    } else {
        struct recycle_msg tmp;
        tmp.badge = proc_badge;
        tmp.exitcode = exitcode;
        set_one_msg(recycle_msg_buffer, &tmp);

#ifndef FBINFER
        /* Put the msg saved in list into the buffer */
        struct recycle_msg_node *msg, *iter_tmp;

        for_each_in_list_safe (msg, iter_tmp, node, &recycle_msg_head) {
            if (!if_buffer_full(recycle_msg_buffer)) {
                list_del(&msg->node);
                struct recycle_msg tmp2;
                tmp2.badge = msg->msg.badge;
                tmp2.exitcode = msg->msg.exitcode;
                set_one_msg(recycle_msg_buffer, &tmp2);
                kfree(msg);
            } else {
                break;
            }
        }
#endif

        /* Nofity the recycle thread through the notification */
        /*
         * The recycle thread's queue lock will only be
         * grabbed by the kernel (in the following signal_notific)
         * and the recycle thread itself.
         * So, try_lock(queue_lock) in signal_notific should not fail.
         */
        int ret;
        ret = signal_notific(recycle_notification);
        BUG_ON(ret != 0);
    }

    unlock(&recycle_buffer_lock);
}

/* All the threads in current_cap_group should exit */
void sys_exit_group(int exitcode)
{
    struct thread *thread;

    kdebug("%s\n", __func__);

    /*
     * Check if the notification has been sent.
     * E.g., a faulting process may trigger sys_exit_group for many times.
     */
    if (atomic_cmpxchg_32((&current_cap_group->notify_recycler), 0, 1) == 0) {
        /*
         * Grap the threads_lock and set the threads state.
         * After that, no new thread will be allocated.
         * see `create_thread` in thread.c
         */
        lock(&current_cap_group->threads_lock);
        for_each_in_list (
            thread, struct thread, node, &(current_cap_group->thread_list)) {
            /* CAS is used in case the state is set to TE_EXITED
             * concurrently */
            atomic_cmpxchg_32((int *)(&thread->thread_ctx->thread_exit_state),
                              TE_RUNNING,
                              TE_EXITING);
        }
        unlock(&current_cap_group->threads_lock);

        notify_user_recycler(current_cap_group->badge, exitcode);
    }

    /* Set the exit state of current_thread: no contention */
    current_thread->thread_ctx->thread_exit_state = TE_EXITING;
    sched();
    eret_to_thread(switch_context());
}

static void recycle_server_shadow_thread(struct ipc_connection *conn,
                                         struct thread *server_thread,
                                         bool recycle_client_state)
{
    struct ipc_server_handler_config *config;

    config =
        (struct ipc_server_handler_config *)server_thread->general_ipc_config;

    if (config->ipc_exit_routine_entry
        && server_thread->thread_ctx->thread_exit_state == TE_RUNNING) {
        BUG_ON(server_thread->thread_ctx->sc);
        /*
         * If the server shadow thread will not exit (e.g. single
         * shadow thread), it should be locked in case other clients
         * perform ipc_call during its exit routine.
         * The server exit routine should call
         * `ipc_exit_routine_reture` syscall to unlock.
         *
         * Otherwise, locking the server thread hurts nothing since
         * it will exit and will not be used by anyone.
         */
        BUG_ON(is_locked(&config->ipc_lock));
        lock(&config->ipc_lock);

        server_thread->thread_ctx->sc =
            kmalloc(sizeof(*server_thread->thread_ctx->sc));
        if (!server_thread->thread_ctx->sc) {
            kwarn("No memory when %s", __func__);
            return;
        }
        server_thread->thread_ctx->sc->budget = DEFAULT_BUDGET;
        server_thread->thread_ctx->sc->prio = DEFAULT_PRIO;
        arch_set_thread_next_ip(server_thread, config->ipc_exit_routine_entry);
        arch_set_thread_stack(server_thread, config->ipc_routine_stack);
        set_thread_arch_spec_state_ipc(server_thread);

        /* See comments in sys_ipc_close_connection */
        if (recycle_client_state) {
            arch_set_thread_arg0(server_thread, config->destructor);
        } else {
            arch_set_thread_arg0(server_thread, 0);
        }

        arch_set_thread_arg1(server_thread, conn->client_badge);
        arch_set_thread_arg2(server_thread, conn->shm.server_shm_uaddr);
        arch_set_thread_arg3(server_thread, conn->shm.shm_size);
        server_thread->thread_ctx->state = TS_INTER;
        vmspace_unmap_range(conn->server_handler_thread->vmspace,
                            conn->shm.server_shm_uaddr,
                            conn->shm.shm_size);
        BUG_ON(sched_enqueue(server_thread));
    } else {
        /*
         * Since the shadow thread does need to be recycled
         * but won't be scheduled afterwards, its thread state
         * should be set to TS_EXIT here.
         */
        server_thread->thread_ctx->state = TS_EXIT;
    }
}

static int __stop_connection(struct ipc_connection *conn)
{
    if (conn->is_valid == OBJECT_STATE_INVALID) {
        return 0;
    }

    if (try_lock(&conn->ownership) != 0) {
        return -EAGAIN;
    }

    /*
     * Mark the connection as invalid.
     * After, the connection will never be used. See sys_ipc_call.
     */
    conn->is_valid = OBJECT_STATE_INVALID;

    return 0;
}

/* Wait onging IPCs to finish and stop new IPCs. */
static void stop_connection(struct object_slot *slot, int *ret)
{
    struct ipc_connection *conn;

    conn = (struct ipc_connection *)slot->object->opaque;

    *ret = __stop_connection(conn);
}

static void __recycle_connection(struct cap_group *cap_group,
                                 struct ipc_connection *conn,
                                 bool client_process_exited)
{
    struct thread *server_thread;

    BUG_ON(conn->is_valid != OBJECT_STATE_INVALID);

    if (conn->client_badge == cap_group->badge) {
        server_thread = conn->server_handler_thread;
        if (server_thread) {
            recycle_server_shadow_thread(
                conn, server_thread, client_process_exited);
            cap_free(server_thread->cap_group, conn->conn_cap_in_server);
            cap_free(server_thread->cap_group, conn->shm.shm_cap_in_server);
        }
    } else {
        /* cap_group is the server side of the connection */
        conn->server_handler_thread = NULL;
        /* Since we need to lock the connection again when
         * the connection owner (client cap_group) is recycled,
         * unlock here.
         * Don't worry: the connection will not be used any more.
         */
        unlock(&conn->ownership);
    }
}

static void recycle_connection(struct cap_group *cap_group,
                               struct object_slot *slot)
{
    struct ipc_connection *conn;

    conn = (struct ipc_connection *)slot->object->opaque;

    __recycle_connection(cap_group, conn, true);
}

/*
 * Close an IPC connection from a client **thread** to a server handler(shadow)
 * thread. It can be invoked by a client thread actively. So, when this syscall
 * is invoked, the client process may have not exited, we should not let IPC
 * server to recycle state stored for client process, because server state is
 * for the whole client but connections are established between two threads in
 * current programming model.
 */
int sys_ipc_close_connection(cap_t connection_cap)
{
    int ret;
    struct ipc_connection *conn;
    struct vmspace *client_vmspace;

    conn = obj_get(current_cap_group, connection_cap, TYPE_CONNECTION);

    if (!conn) {
        ret = -ECAPBILITY;
        goto out;
    }

    ret = __stop_connection(conn);
    if (ret < 0) {
        goto out_put;
    }

    __recycle_connection(current_cap_group, conn, false);

    client_vmspace = current_thread->vmspace;

    vmspace_unmap_range(
        client_vmspace, conn->shm.client_shm_uaddr, conn->shm.shm_size);
    cap_free(current_cap_group, conn->shm.shm_cap_in_client);

    cap_free(current_cap_group, connection_cap);
out_put:
    obj_put(conn);
out:
    return ret;
}

/* Wait onging IPC registration to finish and stop newly coming ones */
static void stop_ipc_registration(struct cap_group *cap_group,
                                  struct object_slot *slot, int *ret)
{
    struct thread *thread;
    struct ipc_server_register_cb_config *config;

    thread = (struct thread *)slot->object->opaque;
    if (thread->cap_group != cap_group)
        return;

    if (thread->thread_ctx->type != TYPE_REGISTER)
        return;

    /* Avoid deadlock during try again */
    if (thread->thread_ctx->thread_exit_state == TE_EXITED)
        return;

    config = (struct ipc_server_register_cb_config *)thread->general_ipc_config;
    if (try_lock(&config->register_lock) != 0) {
        /* Lock fails: registration is ongoing. So, try next time. */
        *ret = -EAGAIN;
        return;
    }

    /*
     * No release the register_lock. So, the register_cb_thread will never
     * execute any more.
     */
    thread->thread_ctx->thread_exit_state = TE_EXITED;
    thread->thread_ctx->state = TS_EXIT;
}

static void stop_notification(struct object_slot *slot)
{
    struct notification *notific;

    notific = (struct notification *)slot->object->opaque;
    lock(&notific->notifc_lock);
    notific->state = OBJECT_STATE_INVALID;
    unlock(&notific->notifc_lock);
}

#ifdef CHCORE_OH_TEE
static void stop_channel(struct object_slot *slot)
{
    struct channel *channel;
    channel = (struct channel *)slot->object->opaque;
    close_channel(channel, slot->cap_group);
}
#endif /* CHCORE_OH_TEE */

/*
 * Convention: sys_exit_group is executed before to notify the recycle thread
 * which then executes sys_cap_group_recycle.
 *
 * If a thread invoke this to recycle the resources, the kernel will run
 * on the thread's kernel stack, which makes things complex.
 * So, only the user-level recycler in the process manager
 * can invoke cap_group_exit on some cap_group.
 *
 * Case-1: a thread invokes exit, it will directly tell the process manager,
 *         and then, the process manager invokes this function.
 * Case-2: if a thread triggers faults (e.g., segfault), the kernel will notify
 *         the process manager to exit the corresponding process (cap_group).
 */
int sys_cap_group_recycle(cap_t cap_group_cap)
{
    struct cap_group *cap_group;
    struct thread *thread;
    int ret;
    struct slot_table *slot_table;
    cap_t slot_id;
    struct vmspace *vmspace = NULL;

    if ((ret = hook_sys_cap_group_recycle(cap_group_cap)) != 0)
        return ret;

    cap_group = obj_get(current_cap_group, cap_group_cap, TYPE_CAP_GROUP);
    if (!cap_group)
        return -ECAPBILITY;

    ret = 0;
    /* Phase-1: Stop all the threads in this cap_group */

    /* IPC recycle begin */
    slot_table = &cap_group->slot_table;
    write_lock(&slot_table->table_guard);

    /* Handle all the connection caps and the register_cb_thread caps */
    for_each_set_bit (slot_id, slot_table->slots_bmp, slot_table->slots_size) {
        struct object_slot *slot;

        slot = get_slot(cap_group, slot_id);
        BUG_ON(slot == NULL);

        if (slot->object->type == TYPE_CONNECTION) {
            stop_connection(slot, &ret);
        } else if (slot->object->type == TYPE_THREAD) {
            stop_ipc_registration(cap_group, slot, &ret);
        } else if (slot->object->type == TYPE_NOTIFICATION) {
            stop_notification(slot);
        }
#ifdef CHCORE_OH_TEE
        if (slot->object->type == TYPE_CHANNEL) {
            stop_channel(slot);
        }
#endif /* CHCORE_OH_TEE */
    }

    write_unlock(&slot_table->table_guard);
    /* IPC recycle end */

    if (ret == -EAGAIN) {
        kdebug("%s: Line: %d\n", __func__, __LINE__);
        goto out;
    }

    /*
     * As `sys_exit_group` is executed before:
     * - no new thread will be created
     * - each thread is set as TE_EXITING in that function
     */
    for_each_in_list (thread, struct thread, node, &(cap_group->thread_list)) {
        /* If some thread is not TE_EXITED, then return -EAGAIN. */
        if (thread->thread_ctx->thread_exit_state != TE_EXITED) {
            /*
             * As all the connection are set to INVALID in previous
             * step, all the shadow threads (IPC server threads)
             * will not execute any more.
             * Thus, we directly set them to exited here.
             */
            if (thread->thread_ctx->type == TYPE_SHADOW) {
                thread->thread_ctx->thread_exit_state = TE_EXITED;
                continue;
            }

            if (thread->thread_ctx->state == TS_WAITING) {
                try_remove_timeout(thread);
                thread->thread_ctx->thread_exit_state = TE_EXITED;
                continue;
            }

            ret = -EAGAIN;
        }
    }

    if (ret == -EAGAIN) {
        kdebug("%s: Line: %d\n", __func__, __LINE__);
        goto out;
    }

    /* All the thread are TE_EXITED now, wait until their kernel stacks are
     * free */
    for_each_in_list (thread, struct thread, node, &(cap_group->thread_list)) {
        wait_for_kernel_stack(thread);
        BUG_ON(thread->thread_ctx->thread_exit_state != TE_EXITED);
        if (thread->thread_ctx->state != TS_EXIT)
            kwarn("thread ctx->state is %d\n", thread->thread_ctx->state);
    }

    /*
     * Phase-2:
     * Iterate all the capability table and free the corresponding
     * resources.
     */

    slot_table = &cap_group->slot_table;
    write_lock(&slot_table->table_guard);

    for_each_set_bit (slot_id, slot_table->slots_bmp, slot_table->slots_size) {
        struct object_slot *slot;
        struct object *object;

        slot = get_slot(cap_group, slot_id);
        BUG_ON(!slot || slot->isvalid == false);
        object = slot->object;

        if (slot_id == VMSPACE_OBJ_ID) {
            vmspace = (struct vmspace *)(object->opaque);
            flush_tlb_by_vmspace(vmspace);
        }

        if (object->type == TYPE_CONNECTION) {
            recycle_connection(cap_group, slot);
            struct ipc_connection *conn =
                (struct ipc_connection *)slot->object->opaque;

            __cap_free(cap_group, conn->shm.shm_cap_in_client, true, false);
            __cap_free(cap_group, slot_id, true, false);
        } else if ((object->type == TYPE_THREAD)
                   && (((struct thread *)(object->opaque))->cap_group
                       == cap_group)) {
            /*
             * Use cap_free_all to free the threads belong to
             * the exited cap_group.
             */
            kdebug("recycle one local thread.\n");

            /*
             * Like cap_free_all, but without locks.
             * Directly using cap_free_all leads to dead lock.
             */
            struct object_slot *slot_iter = NULL, *slot_iter_tmp = NULL;
            int r;

            /* Not using obj_get or get_opaque is also for avoid
             * deadlock. */
            atomic_fetch_add_long(&object->refcount, 1);

            /* free all copied slots */
            lock(&object->copies_lock);
            for_each_in_list_safe (
                slot_iter, slot_iter_tmp, copies, &object->copies_head) {
                int iter_slot_id = slot_iter->slot_id;
                struct cap_group *iter_cap_group = slot_iter->cap_group;

                r = __cap_free(iter_cap_group,
                               iter_slot_id,
                               iter_cap_group == cap_group,
                               true);
                BUG_ON(r != 0);
            }
            unlock(&object->copies_lock);

            obj_put(object->opaque);
        } else {
            __cap_free(cap_group, slot_id, true, false);
        }
    }
    write_unlock(&slot_table->table_guard);

    /* The cap_group will be freed in the following cap_free_all. */
    obj_put(cap_group);
    cap_free_all(current_cap_group, cap_group_cap);

    kdebug("%s is done\n", __func__);

    return ret;
out:
    obj_put(cap_group);
    return ret;
}
