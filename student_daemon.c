
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

#define PID_FILE "/tmp/student_daemon.pid"
#define LOCK_FILE "/tmp/student_daemon.lock"
#define LOG_FILE "/tmp/student_daemon.log"
#define INTERVAL 5

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t reload_requested = 0;

static int lock_fd = -1;
static FILE *log_fp = NULL;

static void handle_sigterm(int sig)
{
    (void)sig;
    running = 0;
}

static void handle_sighup(int sig)
{
    (void)sig;
    reload_requested = 1;
}

static void get_timestamp(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm tm_now;

    if (localtime_r(&now, &tm_now) == NULL) {
        snprintf(buffer, size, "unknown-time");
        return;
    }

    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &tm_now);
}

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

static void daemonize(void)
{
    pid_t pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    if (pid > 0)
        exit(EXIT_SUCCESS);

    if (setsid() < 0)
        exit(EXIT_FAILURE);

    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    if (pid > 0)
        exit(EXIT_SUCCESS);

    umask(0);

    if (chdir("/") < 0)
        exit(EXIT_FAILURE);

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

static int acquire_lock(void)
{
    lock_fd = open(LOCK_FILE, O_RDWR | O_CREAT, 0644);

    if (lock_fd < 0)
        return -1;

    if (flock(lock_fd, LOCK_EX | LOCK_NB) < 0) {
        int saved_errno = errno;
        close(lock_fd);
        lock_fd = -1;

        if (saved_errno == EWOULDBLOCK)
            return 1;

        errno = saved_errno;
        return -1;
    }

    return 0;
}

static int create_pid_file(void)
{
    int fd = open(PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);

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

    daemonize();

    result = acquire_lock();

    if (result != 0)
        return EXIT_FAILURE;

    if (create_pid_file() < 0) {
        close(lock_fd);
        return EXIT_FAILURE;
    }

    log_fp = fopen(LOG_FILE, "a");

    if (log_fp == NULL) {
        unlink(PID_FILE);
        close(lock_fd);
        return EXIT_FAILURE;
    }

    if (install_signal_handlers() < 0) {
        cleanup();
        return EXIT_FAILURE;
    }

    log_message("Daemon started");

    while (running) {
        if (reload_requested) {
            reload_requested = 0;
            log_message("SIGHUP received - configuration reload requested");
        }

        if (!running)
            break;

        log_message("Daemon alive");

        struct timespec req = {INTERVAL, 0};
        struct timespec rem;

        while (nanosleep(&req, &rem) < 0) {
            if (errno != EINTR)
                break;

            if (!running || reload_requested)
                break;

            req = rem;
        }
    }

    log_message("SIGTERM received");
    cleanup();

    return EXIT_SUCCESS;
}
