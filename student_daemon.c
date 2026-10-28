
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#define PID_FILE  "/tmp/student_daemon.pid"
#define LOCK_FILE "/tmp/student_daemon.lock"
#define LOG_FILE  "/tmp/student_daemon.log"

#define INTERVAL 5

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t reload_requested = 0;

static int lock_fd = -1;
static FILE *log_fp = NULL;

/* Handle SIGTERM: request graceful shutdown */
static void handle_sigterm(int sig)
{
    (void)sig;
    running = 0;
}

/* Handle SIGHUP: request reload */
static void handle_sighup(int sig)
{
    (void)sig;
    reload_requested = 1;
}

/* Generate timestamp */
static void get_timestamp(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm tm_now;

    if (localtime_r(&now, &tm_now) == NULL) {
        snprintf(buffer, size, "unknown-time");
        return;
    }

    strftime(buffer, size, "%Y-%m-%d %H:%M:%S",
             &tm_now);
}

/* Write messages to the log file */
static void log_message(const char *format, ...)
{
    char timestamp[64];
    va_list args;

    if (log_fp == NULL)
        return;

    get_timestamp(timestamp, sizeof(timestamp));

    fprintf(log_fp, "%s | PID=%ld | ",
            timestamp, (long)getpid());

    va_start(args, format);
    vfprintf(log_fp, format, args);
    va_end(args);

    fputc('\n', log_fp);
    fflush(log_fp);
}

/* Convert the foreground process into a daemon */
static void daemonize(void)
{
    pid_t pid;

    /* First fork */
    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Create a new session */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Second fork */
    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set file creation mask */
    umask(0);

    /* Change working directory */
    if (chdir("/") < 0)
        exit(EXIT_FAILURE);

    /* Redirect stdin, stdout and stderr */
    int fd = open("/dev/null", O_RDWR);

    if (fd < 0)
        exit(EXIT_FAILURE);

    if (dup2(fd, STDIN_FILENO) < 0 ||
        dup2(fd, STDOUT_FILENO) < 0 ||
        dup2(fd, STDERR_FILENO) < 0) {
        exit(EXIT_FAILURE);
    }

    if (fd > STDERR_FILENO)
        close(fd);
}

/* Acquire exclusive lock to prevent duplicate instances */
static int acquire_lock(void)
{
    lock_fd = open(LOCK_FILE, O_RDWR | O_CREAT, 0644);

    if (lock_fd < 0)
        return -1;

    if (flock(lock_fd, LOCK_EX | LOCK_NB) < 0) {
        close(lock_fd);
        lock_fd = -1;

        if (errno == EWOULDBLOCK)
            return 1;

        return -1;
    }

    return 0;
}

/* Create PID file */
static int create_pid_file(void)
{
    int fd = open(PID_FILE,
                  O_WRONLY | O_CREAT | O_TRUNC,
                  0644);

    if (fd < 0)
        return -1;

    if (dprintf(fd, "%ld\n", (long)getpid()) < 0) {
        close(fd);
        unlink(PID_FILE);
        return -1;
    }

    if (close(fd) < 0) {
        unlink(PID_FILE);
        return -1;
    }

    return 0;
}

/* Install signal handlers */
static int install_signal_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sa.sa_handler = handle_sigterm;

    if (sigaction(SIGTERM, &sa, NULL) < 0)
        return -1;

    sa.sa_handler = handle_sighup;

    if (sigaction(SIGHUP, &sa, NULL) < 0)
        return -1;

    return 0;
}

/* Release resources during shutdown */
static void cleanup(void)
{
    log_message("Daemon shutting down");

    unlink(PID_FILE);

    if (log_fp != NULL) {
        fclose(log_fp);
        log_fp = NULL;
    }

    if (lock_fd >= 0) {
        close(lock_fd);
        lock_fd = -1;
    }
}

int main(void)
{
    int result;

    /* Step 1: Daemonize */
    daemonize();

    /* Step 2: Prevent duplicate instances */
    result = acquire_lock();

    if (result != 0)
        return EXIT_FAILURE;

    /* Step 3: Create PID file */
    if (create_pid_file() < 0) {
        close(lock_fd);
        return EXIT_FAILURE;
    }

    /* Step 4: Open log file */
    log_fp = fopen(LOG_FILE, "a");

    if (log_fp == NULL) {
        unlink(PID_FILE);
        close(lock_fd);
        return EXIT_FAILURE;
    }

    /* Step 5: Install signal handlers */
    if (install_signal_handlers() < 0) {
        cleanup();
        return EXIT_FAILURE;
    }

    log_message("Daemon started");

    /* Step 6: Main daemon loop */
    while (running) {

        /* Process SIGHUP request */
        if (reload_requested) {
            reload_requested = 0;

            log_message(
                "SIGHUP received - configuration reload requested"
            );
        }

        if (!running)
            break;

        /* Perform normal daemon activity */
        log_message("Daemon alive");

        /* Sleep for approximately five seconds */
        struct timespec req = { INTERVAL, 0 };
        struct timespec rem;

        while (nanosleep(&req, &rem) < 0) {
            if (errno != EINTR)
                break;

            if (!running || reload_requested)
                break;

            req = rem;
        }
    }

    /* Step 7: Graceful shutdown */
    log_message("SIGTERM received");
    cleanup();

    return EXIT_SUCCESS;
}

