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
    volatile sig_atomic_t count;
    volatile sig_atomic_t pending; // Flag to indicate a signal needs processing
    volatile sig_atomic_t last_sender_pid; // To record the sender PID safely
    time_t last_time; // Will be updated outside the handler
} signal_record_t;

// Global array to record signals received
signal_record_t signal_records[NSIG];

// Flag to indicate new signals have arrived that need processing
volatile sig_atomic_t signals_pending = 0;

// Minimal, async-signal-safe handler that just records the signal
void handler(int sig, siginfo_t *si, void *ctx) {
    if (sig > 0 && sig < NSIG) {
        // Minimally update atomic counters - safe in handler
        signal_records[sig].count++;
        signal_records[sig].pending = 1;
        signal_records[sig].last_sender_pid = si->si_pid;
        signals_pending = 1;
        
        // Use only async-signal-safe operation: write() with a fixed string
        const char msg[] = "Signal received\n";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    }
}

// Function to process pending signals - called from regular code, not from handler
void process_pending_signals() {
    if (!signals_pending) return;
    
    char buf[256];
    for (int sig = 1; sig < NSIG; sig++) {
        if (signal_records[sig].pending) {
            // Update timestamp - safe to do outside of handler
            signal_records[sig].last_time = time(NULL);
            
            // Now safe to use non-async functions like snprintf and strsignal
            snprintf(buf, sizeof(buf),
                     "[processed] Process %d received signal %d (%s) from PID %d (count: %d)\n",
                     getpid(), sig, strsignal(sig), 
                     signal_records[sig].last_sender_pid, 
                     signal_records[sig].count);
            
            write(STDOUT_FILENO, buf, strlen(buf));
            
            // Clear the pending flag
            signal_records[sig].pending = 0;
        }
    }
    
    // Reset the global flag
    signals_pending = 0;
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
                    // Process any signals received since last iteration
                    process_pending_signals();
                    
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
                    // Process any signals between sending
                    process_pending_signals();
                    
                    send_signal(me, parent_pid, all_sigs[j], 1);
                    sleep(1);  // Wait between different signals
                }
                
                // Send each signal multiple times
                printf("\nNow sending each signal multiple times...\n");
                for (j = 0; j < 8; j++) {
                    // Process any signals between sending
                    process_pending_signals();
                    
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
                    // Process any signals between sending
                    process_pending_signals();
                    
                    send_signal(me, parent_pid, child3_sigs[j], 1);
                    sleep(1);  // Wait between different signals
                }
                
                // Send each signal multiple times
                printf("\nNow sending each signal multiple times to parent...\n");
                for (j = 0; j < 4; j++) {
                    // Process any signals between sending
                    process_pending_signals();
                    
                    send_signal(me, parent_pid, child3_sigs[j], 3);
                    sleep(1);  // Wait between different signals
                }
            }
            
            // Final processing of any pending signals
            process_pending_signals();
            
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
    
    /* parent waits for children to finish, periodically processing pending signals */
    for (i = 0; i < 4; i++) {
        int status;
        pid_t wpid;
        
        // Wait in short bursts to process signals
        while ((wpid = waitpid(pids[i], &status, WNOHANG)) == 0) {
            // Process any signals that arrived
            process_pending_signals();
            usleep(100000); // Sleep 0.1 sec before checking again
        }
        
        if (wpid < 0) {
            perror("waitpid");
        } else {
            printf("Parent: Child %d (PID=%d) has exited\n", i, pids[i]);
        }
        
        // Process signals again after child exit
        process_pending_signals();
    }

    /* restore default handlers and sleep to catch any signals */
    for (i = 0; i < 8; i++)
        signal(all_sigs[i], SIG_DFL);

    printf("Parent: children done; restored defaults, now sleeping 10s\n");
    printf("Use terminal to send signals: kill -SIGNAL %d\n", parent_pid);
    
    // Sleep in short bursts to process signals
    for (i = 0; i < 100; i++) {
        process_pending_signals();
        usleep(100000); // Sleep 0.1 seconds
    }
    
    printf("Parent exiting\n");
    
    print_signal_stats();
    return 0;
} 