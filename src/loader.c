#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <sys/resource.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>

// Increase RLIMIT_MEMLOCK to allow loading larger BPF programs
static int bump_memlock_rlimit(void)
{
    struct rlimit rlim_new = {
        .rlim_cur = RLIM_INFINITY,
        .rlim_max = RLIM_INFINITY,
    };

    if (setrlimit(RLIMIT_MEMLOCK, &rlim_new)) {
        perror("Failed to increase RLIMIT_MEMLOCK");
        return -1;
    }

    return 0;
}

static void print_usage(const char *prog)
{
    printf("Usage: %s [OPTIONS] <ebpf_object_file>\n", prog);
    printf("Options:\n");
    printf("  -a, --add-pid <pid>       Add PID to priority queue\n");
    printf("  -r, --remove-pid <pid>    Remove PID from priority queue\n");
    printf("  -l, --list-pids           List all PIDs in priority queue\n");
    printf("  -s, --stats               Display queue statistics\n");
    printf("  -h, --help                Show this help message\n");
}

int main(int argc, char **argv)
{
    struct bpf_object *obj;
    struct bpf_map *priority_pids_map, *stats_map;
    const char *obj_file;
    int ret = 0, option_index = 0;
    int add_pid = -1, remove_pid = -1, list_pids = 0, show_stats = 0;
    struct option options[] = {
        {"add-pid", required_argument, NULL, 'a'},
        {"remove-pid", required_argument, NULL, 'r'},
        {"list-pids", no_argument, NULL, 'l'},
        {"stats", no_argument, NULL, 's'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, NULL, 0}
    };

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    // Parse options
    int opt;
    while ((opt = getopt_long(argc, argv, "a:r:lsh", options, &option_index)) != -1) {
        switch (opt) {
        case 'a':
            add_pid = atoi(optarg);
            break;
        case 'r':
            remove_pid = atoi(optarg);
            break;
        case 'l':
            list_pids = 1;
            break;
        case 's':
            show_stats = 1;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: No BPF object file specified\n");
        print_usage(argv[0]);
        return 1;
    }

    obj_file = argv[optind];

    // Check if file exists
    if (access(obj_file, F_OK) != 0) {
        fprintf(stderr, "Error: BPF object file not found: %s\n", obj_file);
        return 1;
    }

    // Increase RLIMIT_MEMLOCK
    if (bump_memlock_rlimit()) {
        return 1;
    }

    // Load BPF object
    printf("Loading BPF object: %s\n", obj_file);
    obj = bpf_object__open(obj_file);
    if (libbpf_get_error(obj)) {
        fprintf(stderr, "Failed to open BPF object: %s\n", strerror(errno));
        return 1;
    }

    // Load BPF programs
    ret = bpf_object__load(obj);
    if (ret) {
        fprintf(stderr, "Failed to load BPF object: %s\n", strerror(errno));
        goto cleanup;
    }

    printf("BPF object loaded successfully\n");

    // Get the priority_pids_map
    priority_pids_map = bpf_object__find_map_by_name(obj, "priority_pids_map");
    if (!priority_pids_map) {
        fprintf(stderr, "Error: Could not find priority_pids_map\n");
        ret = 1;
        goto cleanup;
    }

    // Get the queue_stats map
    stats_map = bpf_object__find_map_by_name(obj, "queue_stats");
    if (!stats_map) {
        fprintf(stderr, "Error: Could not find queue_stats map\n");
        ret = 1;
        goto cleanup;
    }

    int map_fd = bpf_map__fd(priority_pids_map);
    int stats_fd = bpf_map__fd(stats_map);

    // Handle add-pid operation
    if (add_pid > 0) {
        __u32 priority_val = 1;  // Mark as priority task
        printf("Adding PID %d to priority queue\n", add_pid);
        ret = bpf_map_update_elem(map_fd, &add_pid, &priority_val, BPF_ANY);
        if (ret) {
            fprintf(stderr, "Failed to add PID to priority queue: %s\n", strerror(errno));
            goto cleanup;
        }
        printf("Successfully added PID %d to priority queue\n", add_pid);
    }

    // Handle remove-pid operation
    if (remove_pid > 0) {
        printf("Removing PID %d from priority queue\n", remove_pid);
        ret = bpf_map_delete_elem(map_fd, &remove_pid);
        if (ret && errno != ENOENT) {
            fprintf(stderr, "Failed to remove PID from priority queue: %s\n", strerror(errno));
            goto cleanup;
        }
        printf("Successfully removed PID %d from priority queue\n", remove_pid);
    }

    // Handle list-pids operation
    if (list_pids) {
        printf("PIDs in priority queue:\n");
        __u32 pid = 0;
        __u32 next_pid;
        __u32 priority_val;

        while (bpf_map_get_next_key(map_fd, &pid, &next_pid) == 0) {
            if (bpf_map_lookup_elem(map_fd, &next_pid, &priority_val) == 0) {
                printf("  PID: %u (priority: %u)\n", next_pid, priority_val);
            }
            pid = next_pid;
        }
    }

    // Handle stats operation
    if (show_stats) {
        printf("Queue Statistics:\n");
        
        // Define stat indices
        #define STAT_PRIORITY_ENQUEUED  0
        #define STAT_BATCH_ENQUEUED     1
        #define STAT_PRIORITY_DISPATCHED 2
        #define STAT_BATCH_DISPATCHED   3
        
        __u32 stat_keys[] = {STAT_PRIORITY_ENQUEUED, STAT_BATCH_ENQUEUED, 
                             STAT_PRIORITY_DISPATCHED, STAT_BATCH_DISPATCHED};
        const char *stat_names[] = {"Priority Enqueued", "Batch Enqueued", 
                                    "Priority Dispatched", "Batch Dispatched"};
        
        for (int i = 0; i < 4; i++) {
            __u64 stats[256];  // Max 256 CPUs
            __u32 key = stat_keys[i];
            
            if (bpf_map_lookup_elem(stats_fd, &key, stats) == 0) {
                // Sum across all CPUs
                __u64 total = 0;
                for (int cpu = 0; cpu < 256; cpu++) {
                    total += stats[cpu];
                }
                printf("  %s: %llu\n", stat_names[i], total);
            }
        }
    }

cleanup:
    bpf_object__close(obj);
    return ret;
}
