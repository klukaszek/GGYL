#include "ggyl.h"
#include <signal.h>
#include <sys/inotify.h>

monitor_t monitor = {-1,
                     ".",
                     "",
                     NULL,
                     0,
                     NULL,
                     IN_MODIFY | IN_CREATE | IN_DELETE | IN_ISDIR |
                         IN_MOVED_FROM};

// Print usage and exit
void usage() {
    fprintf(stderr, "Usage: ggyl [-d directory] cmd [regex_patterns]\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -d directory  Directory to monitor\n");
    fprintf(stderr, "  cmd           Command to execute\n");
    fprintf(
        stderr,
        "  regex_patterns  \"*.c\" \"*.md\" (optional. max 128 patterns)\n");
    exit(EXIT_FAILURE);
}

// Function to convert a glob pattern to a POSIX regex pattern
void glob_to_regex(const char *glob, char *regex) {
    char *p = regex;
    *p++ = '^'; // Start-of-line anchor
    while (*glob) {
        switch (*glob) {
            case '*':
                *p++ = '.';
                *p++ = '*';
                break;
            case '?':
                *p++ = '.';
                break;
            case '.':
                *p++ = '\\';
                *p++ = '.';
                break;
            default:
                *p++ = *glob;
                break;
        }
        glob++;
    }
    *p++ = '$'; // End-of-line anchor
    *p = '\0';
}

// Compile regex patterns and store them in the regex_entries array
// Any invalid regex patterns will be marked as not compiled
void compile_patterns(monitor_t *mon, char *glob) {
    if (mon->num_patterns >= MAX_REGEX) {
        fprintf(stderr, "Too many regex patterns, max is %d\n", MAX_REGEX);
        return;
    }

    // Convert glob pattern to regex pattern
    char regex[MAX_LEN];
    glob_to_regex(glob, regex);

    printf("Compiling regex %s\n", glob);

    // Allocate memory for the regex program
    regex_t *regex_data = (regex_t *)malloc(sizeof(regex_t));
    if (regex_data == NULL) {
        perror("Error: compile_patterns -> malloc");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for the regex entry
    regex_entry *regex_entry_elem = (regex_entry *)malloc(sizeof(regex_entry));
    mon->regex_entries[mon->num_patterns] = regex_entry_elem;

    // Compile the regex pattern and store it in the regex_entries array
    if (regcomp(regex_data, regex, REG_EXTENDED | REG_NOSUB) != 0) {
        fprintf(stderr, "Failed to compile regex %s", glob);
        mon->regex_entries[mon->num_patterns]->compiled = 0;
        free(regex_entry_elem);
        free(regex_data);
    } else {
        mon->regex_entries[mon->num_patterns]->compiled = 1;
        mon->regex_entries[mon->num_patterns]->regex = regex_data;
        mon->num_patterns++;
    }
}

// Check if a filename matches any of the compiled regex patterns
int check_patterns(monitor_t *mon, char *filename) {
    for (int i = 0; i < mon->num_patterns; i++) {
        if (mon->regex_entries[i]->compiled) {
            if (regexec(mon->regex_entries[i]->regex, filename, 0, NULL, 0) ==
                0) {
                return 1; // Match
            }
        }
    }

    // No patterns, match everything
    if (mon->num_patterns == 0) {
        return 1;
    }

    return 0; // No match
}

// Free the regex entries of the monitor
void free_regex_entries(monitor_t *mon) {
    for (int i = 0; i < mon->num_patterns; i++) {
        // Find compiled regex patterns and free them
        if (mon->regex_entries[i]->compiled) {
            regfree(mon->regex_entries[i]->regex);
            free(mon->regex_entries[i]->regex);
        }
        // Free the regex entry
        free(mon->regex_entries[i]);
    }
    // Free the regex_entries array
    free(mon->regex_entries);
}

// Build a tree of watch descriptors for the inotify events
// This tree doesn't necessarily represent the directory structure.
// I should probably make a watch_entry struct to store the watch descriptor and
// the directory name to make it more clear but for now this does what I need it
// to do.
int_tree *build_watch_tree(monitor_t *mon, char *dir, node_t *parent) {
    int_tree *wd_entries = mon->wd_entries;

    // Add the watch descriptor to the tree

    // Open the directory
    DIR *dp = opendir(dir);
    if (dp == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    // Block signals during the creation of the node to avoid leaking memory on
    // close
    sigset_t set, oldset;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGSEGV);
    sigprocmask(SIG_BLOCK, &set, &oldset);

    // Create a new node and figure out if it's the root node or not
    node_t *node = create_node(NULL);
    if (wd_entries->root == NULL) {
        wd_entries->root = node;
    } else {
        _add_child(parent, node);
    }

    // Add the watch descriptor to the directory
    int wd = inotify_add_watch(mon->fd, dir, mon->mask);
    if (wd < 0) {
        perror("inotify_add_watch");
        exit(EXIT_FAILURE);
    }

    // Add the watch descriptor to the node
    node->data = &wd;

    // Unblock signals by restoring the old signal mask
    sigprocmask(SIG_SETMASK, &oldset, NULL);

    // Read the directory entries
    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_type == DT_DIR) {
            // Skip the hidden, current, and parent directories
            if (entry->d_name[0] == '.') {
                continue;
            }

            // Build the full path
            char path[MAX_LEN];
            snprintf(path, MAX_LEN, "%s/%s", dir, entry->d_name);

            build_watch_tree(mon, path, node);
        }
    }

    // Close the directory
    closedir(dp);

    return wd_entries;
}

// Monitor directory and subdirectories for any inotify events on the pipe file
// descriptor This function will be called in an infinite loop to execute the
// command once an event occurs. The monitor->wd_entries tree will be rebuilt if
// destructive IN_ISDIR events occur.
void monitor_directory(monitor_t *mon) {

    // Begin monitoring (only interrupted by signal handler)
    const int buffer_size = 1024 * (sizeof(struct inotify_event) + 16);
    char buffer[buffer_size];
    while (1) {

        // Assign temp file descriptor to the inotify file descriptor
        fd_set fds;
        struct timeval tv;

        FD_ZERO(&fds);
        FD_SET(mon->fd, &fds);

        int debounce_delay = 1;
        tv.tv_sec = debounce_delay;
        tv.tv_usec = 0;

        // Wait for inotify events on the file descriptor
        int ret = select(mon->fd + 1, &fds, NULL, NULL, &tv);
        if (ret < 0) {
            fprintf(stderr, "Failed to select inotify event: %s",
                    strerror(errno));
            exit(EXIT_FAILURE);
        }

        // If select returns successfully
        // Read to clear the inotify buffer and ignore specifics
        if (ret > 0) {
            // Read to clear the inotify buffer and ignore specifics
            read(mon->fd, buffer, buffer_size);

            while (1) {
                FD_ZERO(&fds);
                FD_SET(mon->fd, &fds);
                tv.tv_sec = 0;
                tv.tv_usec = 20000;

                // Wait for inotify events on the file descriptor
                ret = select(mon->fd + 1, &fds, NULL, NULL, &tv);
                if (ret < 0) {
                    fprintf(stderr, "Failed to select inotify event: %s",
                            strerror(errno));
                    break;
                }

                // Read file descriptor to buffer
                read(mon->fd, buffer, buffer_size);

                // get an inotify event from the buffer
                struct inotify_event *event = (struct inotify_event *)buffer;
                if (event->len) {

                    // Rebuid the watch tree if a directory change is noted
                    if (event->mask & IN_ISDIR &&
                        (event->mask & IN_CREATE || event->mask & IN_DELETE ||
                         event->mask & IN_MOVE)) {
                        free_tree(mon->wd_entries);
                        mon->wd_entries = create_tree(
                            int, free_int, compare_int, int_to_str, print_int);
                        mon->wd_entries = build_watch_tree(mon, mon->dir, NULL);
                        system("clear");
                        system(mon->cmd);
                        break;
                    }

                    // If the event is a modify event, check if the event name
                    // matches any of the regex patterns
                    if (event->mask & IN_MODIFY) {
                        if (check_patterns(mon, event->name)) {
                            system("clear");
                            system(mon->cmd);
                            break;
                        }
                    }
                }
            }
        }
    }
}

// Free memory and exit
void handle_signal(int sig) {
    printf("\nggyl: Caught signal %d -> SIG%s\n", sig, sys_siglist[sig]);
    free_regex_entries(&monitor);
    free_tree(monitor.wd_entries);
    close(monitor.fd);
    exit(0);
}

// Program entry point
int main(int argc, char *argv[]) {

    int opt;

    // Parse command line options
    while ((opt = getopt(argc, argv, "d:")) != -1) {
        switch (opt) {
            case 'd':
                strncpy(monitor.dir, optarg, MAX_LEN);
                break;
            default:
                usage();
                exit(EXIT_FAILURE);
        }
    }

    // If no command is provided, print usage and exit
    if (optind >= argc) {
        usage();
        fprintf(stderr, "Expected command after options\n");
        exit(EXIT_FAILURE);
    }

    // Get the command to execute
    strncpy(monitor.cmd, argv[optind], strlen(argv[optind]));
    optind++;

    // Allocate memory for the regex entries
    monitor.regex_entries =
        (regex_entry **)malloc(MAX_REGEX * sizeof(regex_entry *));
    if (monitor.regex_entries == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // Initialize the inotify watch entries array
    monitor.wd_entries =
        create_tree(int, free_int, compare_int, int_to_str, print_int);

    // Get and compile the regex patterns
    while (optind < argc) {
        compile_patterns(&monitor, argv[optind]);
        optind++;
    }

    // Initialize the monitor
    monitor.fd = inotify_init();
    if (monitor.fd < 0) {
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }

    // Initialize the inotify watch for anything in the directory (and
    // subdirectories) Build a tree of inotify watch descriptors, rebuild the
    // tree if the directory changes
    monitor.wd_entries = build_watch_tree(&monitor, monitor.dir, NULL);

    // Assign signal handlers for cleanup since we are using an infinite loop
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGSEGV, handle_signal);

    printf("Monitoring %s\n", monitor.dir);
    printf("Executing %s\n", monitor.cmd);

    ///////// Infinite loop to monitor the directory
    monitor_directory(&monitor);
    ///////// Infinite loop to monitor the directory

    // Standard cleanup (You should never reach this point)
    free_regex_entries(&monitor);
    free_tree(monitor.wd_entries);

    return 0;
}
