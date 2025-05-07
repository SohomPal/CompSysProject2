/*
 * problem1_part2.c
 *
 * Part 2 & Q2: Extends Part 1 to observe signal behavior when:
 * 1) The parent receives signals from terminal
 * 2) Child processes send signals to the parent
 * 3) Signals are sent multiple times in rapid succession
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

// Structure to track signal reception
typedef struct {
    int count;
    time_t last_time;
} signal_record_t;

// Global array to record signals received
signal_record_t signal_records[NSIG];

void record_signal(int sig) {
    signal_records[sig].count++;
    signal_records[sig].last_time = time(NULL);
}

void handler(int sig, siginfo_t *si, void *ctx) {
    pid_t sender_pid = si->si_pid;
    pid_t my_pid = getpid();
    
    record_signal(sig);
    
    // Log which process received the signal and from whom
    char buf[256];
    snprintf(buf, sizeof(buf), 
             "[handler] Process %d received signal %d (%s) from PID %d (count: %d)\n",
             my_pid, sig, strsignal(sig), sender_pid, signal_records[sig].count);
    
    // Use write instead of printf to ensure async-signal safety
    write(STDOUT_FILENO, buf, strlen(buf));
}

void setup_handler(int sig, const sigset_t *mask_during_handler) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_mask = *mask_during_handler;  // block these during handler
    if (sigaction(sig, &sa, NULL) < 0) {
        perror("sigaction");
        exit(1);
    }
}

void print_signal_stats() {
    printf("\n--- Signal Reception Statistics ---\n");
    for (int i = 1; i < NSIG; i++) {
        if (signal_records[i].count > 0) {
            printf("Signal %d (%s): received %d times, last at %s",
                   i, strsignal(i), signal_records[i].count, 
                   ctime(&signal_records[i].last_time));
        }
    }
    printf("--------------------------------\n\n");
}

// Send a signal from one process to another
void send_signal(pid_t sender, pid_t target, int sig, int times) {
    for (int i = 0; i < times; i++) {
        char buf[128];
        snprintf(buf, sizeof(buf), "PID %d sending signal %d to PID %d (%d of %d)\n", 
                 sender, sig, target, i+1, times);
        write(STDOUT_FILENO, buf, strlen(buf));
        
        if (kill(target, sig) < 0) {
            perror("kill");
        }
        
        // Small delay between signals to make them more distinguishable
        usleep(50000);  // 0.05 seconds
    }
}

int main(void) {
    pid_t pids[4];
    pid_t parent_pid = getpid();
    int i, j;
    int all_sigs[] = { SIGINT, SIGABRT, SIGILL, SIGCHLD,
                       SIGSEGV, SIGFPE,  SIGHUP, SIGTSTP };
    
    // Initialize signal recording
    memset(signal_records, 0, sizeof(signal_records));

    /* 1) Parent ignores all eight signals while spawning children */
    for (i = 0; i < 8; i++)
        signal(all_sigs[i], SIG_IGN);

    printf("Parent (PID=%d) started - spawning children\n", parent_pid);
    printf("Send signals to parent via terminal: kill -SIGNAL %d\n", parent_pid);
    
    for (i = 0; i < 4; i++) {
        if ((pids[i] = fork()) == 0) {
            /* child #i */
            pid_t me = getpid();
            printf("Child %d (pid=%d) starting\n", i, me);

            /* choose 4 signals to handle */
            int to_handle[4] = { SIGINT, SIGABRT, SIGILL, SIGSEGV };

            /* permanently block 2 signals */
            sigset_t always_block;
            sigemptyset(&always_block);
            sigaddset(&always_block, SIGFPE);
            sigaddset(&always_block, SIGHUP);
            if (sigprocmask(SIG_BLOCK, &always_block, NULL) < 0) {
                perror("sigprocmask");
                exit(1);
            }

            /* mask another 2 signals during the handler */
            sigset_t handler_mask;
            sigemptyset(&handler_mask);
            sigaddset(&handler_mask, SIGCHLD);
            sigaddset(&handler_mask, SIGTSTP);

            /* install handlers */
            for (j = 0; j < 4; j++) {
                setup_handler(to_handle[j], &handler_mask);
            }
            
            /* Child-specific behavior for signal testing */
            
            // Child 0 and 1 compute as before
            if (i < 2) {
                /* compute sum 0..10*pid, sleeping 1s each iteration */
                long sum = 0;
                // Use a smaller, fixed limit based on child index, not PID
                long limit = 10 * (i + 1);  // 10 or 20 iterations
                
                printf("Child %d (pid=%d) will compute sum from 0 to %ld\n", 
                      i, me, limit);
                
                for (long k = 0; k <= limit; k++) {
                    sum += k;
                    printf("  Child %d (pid=%d): k=%2ld, sum=%10ld\n", i, me, k, sum);
                    sleep(1);
                }
                printf("Child %d (pid=%d) done, final sum=%ld\n", i, me, sum);
            }
            
            // Child 2 sends signals to parent
            else if (i == 2) {
                printf("Child 2 will send signals to Parent (PID=%d)\n", parent_pid);
                sleep(5);  // Give a short delay
                
                // Send each signal once
                for (j = 0; j < 8; j++) {
                    send_signal(me, parent_pid, all_sigs[j], 1);
                    sleep(1);  // Wait between different signals
                }
                
                // Send each signal multiple times
                printf("\nNow sending each signal multiple times...\n");
                for (j = 0; j < 8; j++) {
                    send_signal(me, parent_pid, all_sigs[j], 3);
                    sleep(1);  // Wait between different signals
                }
            }
            
            // Child 3 also sends signals to parent (but a different subset)
            else if (i == 3) {
                printf("Child 3 will send signals to Parent (PID=%d)\n", parent_pid);
                sleep(20);  // Wait for Child 2's signals to be processed
                
                // Send a different subset of signals to demonstrate child-to-parent signaling
                int child3_sigs[] = { SIGINT, SIGILL, SIGHUP, SIGTSTP };
                
                // Send each signal once
                printf("Child 3 sending different subset of signals\n");
                for (j = 0; j < 4; j++) {
                    send_signal(me, parent_pid, child3_sigs[j], 1);
                    sleep(1);  // Wait between different signals
                }
                
                // Send each signal multiple times
                printf("\nNow sending each signal multiple times to parent...\n");
                for (j = 0; j < 4; j++) {
                    send_signal(me, parent_pid, child3_sigs[j], 3);
                    sleep(1);  // Wait between different signals
                }
            }
            
            printf("Child %d (pid=%d) done\n", i, me);
            print_signal_stats();
            exit(0);
        }
    }

    /* Parent installs handlers after children are forked */
    printf("Parent now installing signal handlers\n");
    
    /* mask some signals during the handler */
    sigset_t handler_mask;
    sigemptyset(&handler_mask);
    sigaddset(&handler_mask, SIGCHLD);
    sigaddset(&handler_mask, SIGTSTP);
    
    /* install handlers for all signals */
    for (i = 0; i < 8; i++) {
        setup_handler(all_sigs[i], &handler_mask);
    }
    
    printf("Parent waiting for children (send signals with: kill -SIGNAL %d)\n", parent_pid);
    
    /* parent waits for children to finish */
    for (i = 0; i < 4; i++) {
        waitpid(pids[i], NULL, 0);
        printf("Parent: Child %d (PID=%d) has exited\n", i, pids[i]);
    }

    /* restore default handlers and sleep to catch any signals */
    for (i = 0; i < 8; i++)
        signal(all_sigs[i], SIG_DFL);

    printf("Parent: children done; restored defaults, now sleeping 10s\n");
    printf("Use terminal to send signals: kill -SIGNAL %d\n", parent_pid);
    sleep(10);
    printf("Parent exiting\n");
    
    print_signal_stats();
    return 0;
} 