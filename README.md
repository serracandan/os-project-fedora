# Processes & Threads in Fedora Linux

This project covers process management, thread creation, and synchronization mechanisms using the C programming language in a native Fedora Linux environment.

## Project Overview
The program consists of 5 main sections, demonstrating key operating system concepts directly traced from the Linux kernel data structures via the `/proc` virtual file system.

1. **Process Creation & `/proc` Data**: Forking processes and monitoring state (`/proc/[PID]/status`).
2. **IPC via Unnamed Pipe**: One-way inter-process communication with proper file descriptor management.
3. **Thread Creation & Linux TID**: POSIX thread spawning vs. Native Linux Thread ID tracking (`syscall(SYS_gettid)`).
4. **Race Condition vs. Mutex Synchronization**: Demonstration of data loss under concurrent access and preventing it using `pthread_mutex`.
5. **Producer-Consumer Pattern**: Bounded-buffer management using POSIX semaphores and mutexes.

## Compilation and Execution
To compile and run the project on Fedora Linux (GCC 13+), include the POSIX thread library flag:

```bash
gcc -Wall -Wextra -o os_project os_project.c -lpthread
./os_project

## Technical Summary Table
| Section | System Call / API | Fedora/Linux Feature |
| :--- | :--- | :--- |
| Process Creation | `fork()`, `wait()`, `waitpid()` | Reading `/proc/[PID]/status` |
| Pipe IPC | `pipe()`, `read()`, `write()` | Anonymous FIFO buffer |
| Thread Creation | `pthread_create()`, `pthread_join()` | `syscall(SYS_gettid)`, `/proc/self/task/` |
| Mutex Synchronization | `pthread_mutex_lock() / unlock()` | Race condition determination |
| Producer-Consumer | `sem_init()`, `sem_wait()`, `sem_post()` | POSIX unnamed semaphore |


## Experimental Results
Main Process PID: 5903 (Subprocesses: 5904, 5905)
Race Condition Test (4 threads x 200,000 increments):
Unsafe (No Mutex): Expected: 800,000 | Actual: 686,087 | Lost: 113,913 (Race condition proven)
Safe (With Mutex): Expected: 800,000 | Actual: 800,000 | Lost: 0 (Race condition prevented)

Course Project for ACM369 - Operating Systems I (Spring 2026) 
