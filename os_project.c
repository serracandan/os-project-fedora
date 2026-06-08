

#define _XOPEN_SOURCE 600   
#define _GNU_SOURCE         

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>    
#include <pthread.h>
#include <semaphore.h>

#define RED     "\033[0;31m"
#define GREEN   "\033[0;32m"
#define YELLOW  "\033[0;33m"
#define BLUE    "\033[0;34m"
#define MAGENTA "\033[0;35m"
#define CYAN    "\033[0;36m"
#define BOLD    "\033[1m"
#define RESET   "\033[0m"

#define SEP  "============================================================\n"
#define SUB  "------------------------------------------------------------\n"

static void section_header(int num, const char *title) {
    printf("\n" SEP);
    printf(BOLD CYAN "  SECTION %d: %s\n" RESET, num, title);
    printf(SEP);
}


static void print_linux_proc_info(const char *prefix, const char *color, pid_t id, int is_thread) {
    char path[256];
    char line[256];
    FILE *fp;

    /* If it's a thread, Linux maps it under /proc/self/task/[TID]/status */
    if (is_thread) {
        snprintf(path, sizeof(path), "/proc/self/task/%d/status", id);
    } else {
        snprintf(path, sizeof(path), "/proc/%d/status", id);
    }

    fp = fopen(path, "r");
    if (!fp) {
        printf("%s%s [Linux /proc] Warning: Could not open %s\n" RESET, color, prefix, path);
        return;
    }

    printf("%s%s [Linux OS Data] Reading from: %s\n" RESET, color, prefix, path);
    
    while (fgets(line, sizeof(line), fp)) {
        /* Filter and print specific interesting Linux OS metrics */
        if (strncmp(line, "State:", 6) == 0 ||
            strncmp(line, "Threads:", 8) == 0 ||
            strncmp(line, "Voluntary_ctxt_switches:", 24) == 0) {
            
            line[strcspn(line, "\n")] = 0; /* remove newline */
            printf("%s%s      ↳ %s\n" RESET, color, prefix, line);
        }
    }
    fclose(fp);
}

/* SECTION 1 */
#define NUM_CHILDREN 2

static void demo_process_creation(void)
{
    section_header(1, "Process Creation & Linux /proc Data");

    pid_t pid;
    int   i;

    /* Show main process info first */
    printf(BOLD "  [Main Process] PID: %d\n" RESET, getpid());
    print_linux_proc_info("  ", BOLD, getpid(), 0);
    printf("\n");

    for (i = 0; i < NUM_CHILDREN; i++) {
        pid = fork();

        if (pid < 0) {
            fprintf(stderr, RED "fork() error: %s\n" RESET, strerror(errno));
            exit(EXIT_FAILURE);

        } else if (pid == 0) {
            /* ── Child process ── */
            printf(GREEN "  [Child  #%d] PID: %d  |  Parent PID: %d  |  Working...\n" RESET, 
                   i + 1, getpid(), getppid());
            
            /* Linux Specific: Let the child read its own /proc status */
            print_linux_proc_info("  ", GREEN, getpid(), 0);
            
            sleep(1);   /* simulate work */
            printf(GREEN "  [Child  #%d] PID: %d  |  Done.\n" RESET, i + 1, getpid());
            exit(EXIT_SUCCESS);  /* child stops here */

        } else {
            /* ── Parent process ── */
            printf(YELLOW "  [Parent   ] PID: %d  |  Forked child #%d  →  PID: %d\n" RESET, 
                   getpid(), i + 1, pid);
        }
    }

    /* Parent waits for all children to finish */
    int    status;
    pid_t  wpid;
    while ((wpid = wait(&status)) > 0) {
        if (WIFEXITED(status))
            printf(YELLOW "  [Parent   ] Child PID %d exited  (status=%d)\n" RESET, 
                   wpid, WEXITSTATUS(status));
    }
    printf(YELLOW "  [Parent   ] All children have finished.\n" RESET);
}

/* SECTION 2 */
static void demo_pipe_ipc(void)
{
    section_header(2, "IPC via Unnamed Pipe");

    int   pipefd[2];
    pid_t pid;
    char  msg[]    = "Hello from Parent! [OS Project – IPC Demo]";
    char  buf[128] = {0};

    if (pipe(pipefd) == -1) {
        fprintf(stderr, RED "pipe() error: %s\n" RESET, strerror(errno));
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, RED "fork() error: %s\n" RESET, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        close(pipefd[1]);
        ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
        buf[n] = '\0';
        printf(GREEN   "  [Child   ] Received → \"%s\"\n" RESET, buf);
        close(pipefd[0]);
        exit(EXIT_SUCCESS);
    } else {
        close(pipefd[0]);
        printf(YELLOW  "  [Parent  ] Sending  → \"%s\"\n" RESET, msg);
        write(pipefd[1], msg, strlen(msg));
        close(pipefd[1]);
        wait(NULL);
        printf(YELLOW  "  [Parent  ] IPC demo complete.\n" RESET);
    }
}

/* SECTION 3 */
#define NUM_THREADS 2

typedef struct {
    int thread_id;
    int iterations;
} ThreadArgs;

static void *thread_worker(void *arg)
{
    ThreadArgs *a   = (ThreadArgs *)arg;
    long        sum = 0;
    int         i;

    /* Linux Specific: Get native Linux OS Thread ID (LWP) */
    pid_t os_tid = syscall(SYS_gettid);

    printf(MAGENTA "\n  [Thread %d] POSIX TID: %lu  |  Linux OS TID: %d\n" RESET,
           a->thread_id, (unsigned long)pthread_self(), os_tid);

    /* Read Linux specific thread data from /proc */
    print_linux_proc_info("  ", MAGENTA, os_tid, 1);

    for (i = 0; i < a->iterations; i++)
        sum += i;

    printf(MAGENTA "  [Thread %d] Linux OS TID: %d  |  Sum = %ld  |  Done.\n" RESET,
           a->thread_id, os_tid, sum);

    return NULL;
}

static void demo_thread_creation(void)
{
    section_header(3, "Thread Creation & Linux /proc/self/task");

    pthread_t  tids[NUM_THREADS];
    ThreadArgs args[NUM_THREADS];
    int        i;

    for (i = 0; i < NUM_THREADS; i++) {
        args[i].thread_id  = i + 1;
        args[i].iterations = (i + 1) * 10000;
        int rc = pthread_create(&tids[i], NULL, thread_worker, &args[i]);
        if (rc != 0) {
            fprintf(stderr, RED "pthread_create error: %s\n" RESET, strerror(rc));
            exit(EXIT_FAILURE);
        }
    }

    for (i = 0; i < NUM_THREADS; i++)
        pthread_join(tids[i], NULL);

    printf(MAGENTA "\n  [Main    ] All %d threads joined.\n" RESET, NUM_THREADS);
}

/* SECTION 4 */
#define RACE_THREADS 4
#define INCREMENTS   200000

static long             counter_unsafe = 0;
static long             counter_safe   = 0;
static pthread_mutex_t  mtx            = PTHREAD_MUTEX_INITIALIZER;

static void *inc_unsafe(void *arg) {
    (void)arg;
    int i;
    for (i = 0; i < INCREMENTS; i++)
        counter_unsafe++;
    return NULL;
}

static void *inc_safe(void *arg) {
    (void)arg;
    int i;
    for (i = 0; i < INCREMENTS; i++) {
        pthread_mutex_lock(&mtx);
        counter_safe++;
        pthread_mutex_unlock(&mtx);
    }
    return NULL;
}

static void demo_mutex(void)
{
    section_header(4, "Race Condition vs. Mutex Synchronization");

    pthread_t  tids[RACE_THREADS];
    int        i;
    long       expected = (long)RACE_THREADS * INCREMENTS;

    /* ── Without mutex ── */
    printf(RED "\n  [UNSAFE] %d threads × %d increments (no mutex)…\n" RESET, RACE_THREADS, INCREMENTS);
    counter_unsafe = 0;
    for (i = 0; i < RACE_THREADS; i++)
        pthread_create(&tids[i], NULL, inc_unsafe, NULL);
    for (i = 0; i < RACE_THREADS; i++)
        pthread_join(tids[i], NULL);
    printf(RED
           "  [UNSAFE] Expected : %ld\n"
           "           Actual   : %ld\n"
           "           Lost     : %ld  ← race condition!\n" RESET,
           expected, counter_unsafe, expected - counter_unsafe);

    /* ── With mutex ── */
    printf(GREEN "\n  [SAFE  ] %d threads × %d increments (with mutex)…\n" RESET, RACE_THREADS, INCREMENTS);
    counter_safe = 0;
    for (i = 0; i < RACE_THREADS; i++)
        pthread_create(&tids[i], NULL, inc_safe, NULL);
    for (i = 0; i < RACE_THREADS; i++)
        pthread_join(tids[i], NULL);
    printf(GREEN
           "  [SAFE  ] Expected : %ld\n"
           "           Actual   : %ld\n"
           "           Lost     : %ld  ← no race!\n" RESET,
           expected, counter_safe, expected - counter_safe);

    pthread_mutex_destroy(&mtx);
}

/* SECTION 5 */
#define BUFFER_SIZE 5
#define NUM_ITEMS   12

static int             buffer[BUFFER_SIZE];
static int             buf_in  = 0;
static int             buf_out = 0;
static sem_t           sem_empty;
static sem_t           sem_full;
static pthread_mutex_t buf_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *producer(void *arg) {
    (void)arg;
    int i;
    for (i = 1; i <= NUM_ITEMS; i++) {
        sem_wait(&sem_empty);
        pthread_mutex_lock(&buf_mutex);
        buffer[buf_in] = i;
        buf_in = (buf_in + 1) % BUFFER_SIZE;
        printf(YELLOW "  [Producer] item=%2d  |  buf_in=%d\n" RESET, i, buf_in);
        pthread_mutex_unlock(&buf_mutex);
        sem_post(&sem_full);
        usleep(50000);
    }
    return NULL;
}

static void *consumer(void *arg) {
    (void)arg;
    int i, item;
    for (i = 0; i < NUM_ITEMS; i++) {
        sem_wait(&sem_full);
        pthread_mutex_lock(&buf_mutex);
        item = buffer[buf_out];
        buf_out = (buf_out + 1) % BUFFER_SIZE;
        printf(BLUE   "  [Consumer] item=%2d  |  buf_out=%d\n" RESET, item, buf_out);
        pthread_mutex_unlock(&buf_mutex);
        sem_post(&sem_empty);
        usleep(80000);
    }
    return NULL;
}

static void demo_producer_consumer(void)
{
    section_header(5, "Producer-Consumer with POSIX Semaphores");

    sem_init(&sem_empty, 0, BUFFER_SIZE);
    sem_init(&sem_full,  0, 0);

    pthread_t prod, cons;
    pthread_create(&prod, NULL, producer, NULL);
    pthread_create(&cons, NULL, consumer, NULL);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    sem_destroy(&sem_empty);
    sem_destroy(&sem_full);
    pthread_mutex_destroy(&buf_mutex);

    printf(CYAN "\n  [Main    ] Producer-Consumer demo complete.\n" RESET);
}

/* MAIN */
int main(void)
{
    printf("\n" SEP);
    printf(BOLD "  Processes & Threads in Fedora Linux\n" RESET);
    printf(     "  Operating Systems Course Project\n");
    printf(     "  Main process PID: %d\n", getpid());
    printf(SEP);

    demo_process_creation();
    demo_pipe_ipc();
    demo_thread_creation();
    demo_mutex();
    demo_producer_consumer();

    printf("\n" SEP);
    printf(GREEN BOLD "  All demonstrations completed successfully.\n" RESET);
    printf(SEP "\n");

    return EXIT_SUCCESS;
}