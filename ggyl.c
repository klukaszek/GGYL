#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <unistd.h>

#define MAX_LEN 1024
#define MAX_REGEX 1024
#define MAX_WATCHES 1024

typedef struct {
  regex_t regex;
  int compiled;
} regex_entry;

typedef struct {
  int wd;
  char *dir;
} watch_entry;

typedef struct {
  int fd;
  char dir[MAX_LEN];
  char cmd[MAX_LEN];
  uint32_t mask;
} monitor_t;

// Global regex entries & watch entries
regex_entry regex_entries[MAX_REGEX];
watch_entry watch_entries[MAX_WATCHES];
int num_patterns;
int num_watches;

// Global monitor
monitor_t monitor = {-1, ".", "", IN_MODIFY | IN_CREATE | IN_DELETE};

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
void compile_patterns(char *glob) {
  if (num_patterns >= MAX_REGEX) {
    fprintf(stderr, "Too many regex patterns, max is %d\n", MAX_REGEX);
    return;
  }

  // Convert glob pattern to regex pattern
  char regex[MAX_LEN];
  glob_to_regex(glob, regex);

  printf("Compiling regex %s\n", glob);
  if (regcomp(&regex_entries[num_patterns].regex, regex,
              REG_EXTENDED | REG_NOSUB) != 0) {
    fprintf(stderr, "Failed to compile regex %s", glob);
    regex_entries[num_patterns].compiled = 0;
  } else {
    regex_entries[num_patterns].compiled = 1;
    num_patterns++;
  }
}

// Check if a filename matches any of the compiled regex patterns
int check_patterns(char *filename) {
  for (int i = 0; i < num_patterns; i++) {
    if (regex_entries[i].compiled) {
      if (regexec(&regex_entries[i].regex, filename, 0, NULL, 0) == 0) {
        return 1; // Match
      }
    }
  }

  // No patterns, match everything
  if (num_patterns == 0) {
    return 1;
  }

  return 0; // No match
}

// Once the inotify file descriptor is set up and directories are watched, this
// function will monitor the directories for changes and execute the command if
// any of the regex patterns match
int monitor_directory(monitor_t mon) {

  // Begin monitoring (only interrupted bn signal handler)
  const int buffer_size = 1024 * (sizeof(struct inotify_event) + 16);
  char buffer[buffer_size];
  while (1) {

    // Assign temp file descriptor to the inotify file descriptor
    fd_set fds;
    struct timeval tv;

    FD_ZERO(&fds);
    FD_SET(monitor.fd, &fds);

    int debounce_delay = 1;

    // Set up timeout
    tv.tv_sec = debounce_delay;
    tv.tv_usec = 0;

    // Wait for inotify events on the file descriptor
    int ret = select(monitor.fd + 1, &fds, NULL, NULL, &tv);
    if (ret < 0) {
      fprintf(stderr, "Failed to select inotify event: %s", strerror(errno));
      exit(EXIT_FAILURE);
    }

    // If select returns successfully
    if (ret > 0) {

      // Read to clear the inotify buffer and ignore specifics
      read(monitor.fd, buffer, buffer_size);

      // After the first event, reset the timer and flush the buffer to prevent
      // multiple events from triggering the command
      while (1) {
        FD_ZERO(&fds);
        FD_SET(monitor.fd, &fds);
        tv.tv_sec = 0;       // Shorter wait during the debounce period
        tv.tv_usec = 200000; // 200 milliseconds

        ret = select(monitor.fd + 1, &fds, NULL, NULL, &tv);
        if (ret <= 0)
          break; // Break if timed out or error

        // Continue reading out all events to clear the buffer
        read(monitor.fd, buffer, buffer_size);

        // If the event is a create, modify, or delete event, check if the
        // event name matches any of the regex patterns
        struct inotify_event *event = (struct inotify_event *)buffer;
        if (event->len && (event->mask & IN_CREATE || event->mask & IN_MODIFY ||
                           event->mask & IN_DELETE)) {
          // Check if the filename matches any of the regex patterns
          if (check_patterns(event->name)) {
            system(monitor.cmd);
            sleep(debounce_delay);
            break;
          }
        }
      }
    }
    // If select timed out, continue
  }
}

// Attach inotify watch to a directory and point it to a file descriptor
int add_watch(monitor_t mon, char *dir) {
  int wd =
      inotify_add_watch(mon.fd, dir, IN_CREATE | IN_MODIFY | IN_DELETE);
  if (wd == -1) {
    fprintf(stderr, "Failed to add watch to %s: %s", dir, strerror(errno));
    return -1;
  }

  watch_entries[num_watches].wd = wd;
  watch_entries[num_watches].dir = strdup(dir);
  num_watches++;

  return 1;
}

// Attach inotify watches
int add_watch_rec(monitor_t mon, char *root) {

  // PWD
  DIR *dir = opendir(root);
  if (dir == NULL) {
    fprintf(stderr, "Failed to open directory %s: %s", root,
            strerror(errno));
    return -1;
  }

  struct dirent *entry;

  int flag = 0;

  // Iterate over directory contents
  while ((entry = readdir(dir)) != NULL) {

    // If the current entry is a directory, we want to recursively call this
    // function to add another watch to the file descriptor
    if (entry->d_type == DT_DIR) {

      // Indicate that we successfully found a directory
      flag = 1;

      char path[MAX_LEN];
      // Skip "current" and "parent" directories
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
        continue;
      }

      // Construct the full path to the directory
      snprintf(path, MAX_LEN * 2, "%s/%s", root, entry->d_name);

      // Recursively call this function to add inotify watches to nested
      // directories
      add_watch_rec(mon, path);
    }
  }

  closedir(dir);

  // Add a watch to the current directory
  add_watch(mon, root);

  return 1;
}

// Free all compiled regex patterns
void free_regex() {
  for (int i = 0; i < num_patterns; i++) {
    if (regex_entries[i].compiled) {
      regfree(&regex_entries[i].regex);
    }
  }
}

// Free all watch entries
void free_watches() {
  for (int i = 0; i < MAX_WATCHES; i++) {
    if (watch_entries[i].dir != NULL) {
      free(watch_entries[i].dir);
    }
  }
}

// Free memory and exit
void handle_signal(int sig) {
  printf("\nCaught signal %d -> SIG%s\n", sig, sys_siglist[sig]);
  free_regex();
  free_watches();
  close(monitor.fd);
  exit(0);
}

// Print usage and exit
void usage() {
  fprintf(stderr, "Usage: ggyl [-d directory] cmd [regex_patterns...]\n");
  exit(EXIT_FAILURE);
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

  // Get the command to run
  strncpy(monitor.cmd, argv[optind], strlen(argv[optind]));
  optind++;

  // Compile regex patterns
  while (optind < argc) {
    compile_patterns(argv[optind]);
    optind++;
  }

  // Initialize inotify file descriptor
  monitor.fd = inotify_init();
  if (monitor.fd < 0) {
    perror("inotify_init");
    exit(EXIT_FAILURE);
  }

  printf("Monitoring %s\n", monitor.dir);
  printf("Executing %s\n", monitor.cmd);

  // Signal handling
  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  // Initialize monitor_t
  add_watch_rec(monitor, monitor.dir);

  // Monitor directories for changes (main loop)
  monitor_directory(monitor);

  return 0;
}
