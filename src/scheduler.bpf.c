#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "GPL";

// BPF Map: stores PIDs that should receive priority
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10000);
    __type(key, __u32);
    __type(value, __u32);
} priority_pids_map SEC(".maps");

// Statistics map
struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 4);
    __type(key, __u32);
    __type(value, __u64);
} queue_stats SEC(".maps");

#define STAT_PRIORITY_ENQUEUED  0
#define STAT_BATCH_ENQUEUED     1
#define STAT_PRIORITY_DISPATCHED 2
#define STAT_BATCH_DISPATCHED   3

// Enqueue hook - called when task becomes runnable
void enqueue(struct task_struct *p, u64 enq_flags)
{
    __u32 pid = p->pid;
    __u32 key = 0;
    __u64 *stat_ptr;
    
    // Check if this PID should have priority
    if (bpf_map_lookup_elem(&priority_pids_map, &pid)) {
        key = STAT_PRIORITY_ENQUEUED;
    } else {
        key = STAT_BATCH_ENQUEUED;
    }
    
    stat_ptr = bpf_map_lookup_elem(&queue_stats, &key);
    if (stat_ptr) {
        __sync_fetch_and_add(stat_ptr, 1);
    }
    
    // Dispatch all tasks to local CPU queue
    scx_bpf_dispatch(p, SCX_DSQ_LOCAL, SCX_SLICE_DFL, enq_flags);
}

// Dispatch hook - decides which task to run
void dispatch(s32 cpu, struct task_struct *prev)
{
    // In this simple implementation, we let the kernel's default scheduling handle dispatch
    // The priority is managed through the enqueue hook which places tasks on local queues
    if (!scx_bpf_consume(SCX_DSQ_GLOBAL)) {
        // No global tasks, let the CPU go idle
        return;
    }
}

// Exit task hook - cleanup when task exits
void exit_task(struct task_struct *p, struct scx_exit_task_args *args)
{
    __u32 pid = p->pid;
    bpf_map_delete_elem(&priority_pids_map, &pid);
}

// Structure defining the scheduler operations
SEC("struct_ops/sched_ext")
struct sched_ext_ops scheduler_ops = {
    .enqueue = enqueue,
    .dispatch = dispatch,
    .exit_task = exit_task,
    .name = "priority_scheduler",
};
