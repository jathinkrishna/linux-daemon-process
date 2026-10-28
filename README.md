
# Linux Daemon Process in C

## Project Overview

This project implements a Linux daemon process using C programming and Linux system calls. The daemon runs in the background, periodically writes log messages, handles signals, and manages its PID file.

## Objectives

- Create a daemon using double fork.
- Detach the process from the controlling terminal.
- Create a new session using `setsid()`.
- Manage process IDs using a PID file.
- Prevent duplicate instances using `flock()`.
- Write timestamped messages to a log file.
- Handle `SIGTERM` for graceful shutdown.
- Handle `SIGHUP` for a reload event.
- Clean up resources during termination.

## Technologies Used

- C Programming
- Linux / Ubuntu
- GCC Compiler
- Linux system calls
- Git and GitHub

## Project Structure

    student_daemon/
    ├── student_daemon.c
    ├── Makefile
    ├── README.md
    └── .gitignore

## Compilation

Using GCC:

    gcc -Wall -Wextra -O2 student_daemon.c -o student_daemon

Or using Make:

    make

## Run the Daemon

    ./student_daemon

The daemon detaches from the terminal and continues running in the background.

## Check the Process

    pgrep -a student_daemon

## Check the PID

    cat /tmp/student_daemon.pid

## Monitor Logs

    tail -f /tmp/student_daemon.log

The daemon writes a log entry approximately every five seconds.

## Send SIGHUP

    kill -HUP $(cat /tmp/student_daemon.pid)

This requests a reload event, which is recorded in the log.

## Stop the Daemon

    kill -TERM $(cat /tmp/student_daemon.pid)

The daemon should terminate gracefully and remove its PID file.

## Testing

The project can be tested for:

1. Successful compilation.
2. Background execution.
3. Terminal detachment.
4. Periodic logging.
5. PID file creation.
6. SIGHUP handling.
7. SIGTERM handling.
8. Duplicate-instance prevention.
9. Resource cleanup.
10. Restart after shutdown.

## Learning Outcomes

This project demonstrates Linux process management, daemonization, sessions, file descriptors, signal handling, file locking, logging, and graceful shutdown.
