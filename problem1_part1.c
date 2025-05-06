/*
 * problem1_part1.c
 *
 * Part 1: fork 4 children, each installs handlers for four signals,
 * blocks two signals permanently, blocks two more only during handler,
 * computes sum 0…10×pid sleeping 1 sec each iteration.
 * Parent ignores signals until children finish, then restores defaults
 * and sleeps 10 seconds to catch any signals.
 */

 #define _GNU_SOURCE
 #include <stdio.h>
 #include <stdlib.h>
 #include <unistd.h>
 #include <signal.h>
 #include <sys/wait.h>
 #include <string.h>
 
 void handler(int sig, siginfo_t *si, void *ctx) {
     pid_t pid = getpid();
     printf("  [handler] process %d got signal %d (%s)\n",
            pid, sig, strsignal(sig));
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
 
 int main(void) {
     pid_t pids[4];
     int i, j;
     int all_sigs[] = { SIGINT, SIGABRT, SIGILL, SIGCHLD,
                        SIGSEGV, SIGFPE,  SIGHUP, SIGTSTP };
 
     /* 1) Parent ignores all eight signals while spawning children */
     for (i = 0; i < 8; i++)
         signal(all_sigs[i], SIG_IGN);
 
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
 
             /* compute sum 0..10*pid, sleeping 1s each iteration */
             long sum = 0;
             long limit = 10 * me;
             for (long k = 0; k <= limit; k++) {
                 sum += k;
                 printf("  Child %d (pid=%d): k=%2ld, sum=%10ld\n",
                        i, me, k, sum);
                 sleep(1);
             }
             printf("Child %d (pid=%d) done\n", i, me);
             exit(0);
         }
     }
 
     /* parent waits for children to finish */
     for (i = 0; i < 4; i++) {
         waitpid(pids[i], NULL, 0);
     }
 
     /* restore default handlers and sleep to catch any signals */
     for (i = 0; i < 8; i++)
         signal(all_sigs[i], SIG_DFL);
 
     printf("Parent: children done; restored defaults, now sleeping 10s\n");
     sleep(10);
     printf("Parent exiting\n");
     return 0;
 }
 