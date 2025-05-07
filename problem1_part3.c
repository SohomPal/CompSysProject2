/*
 * problem1_part3.c  (macOS‐patched)
 *
 * Q3: Parent blocks SIGINT, SIGQUIT, SIGTSTP before fork,
 *     installs handlers for the nine signals your script sends,
 *     and now also ignores SIGTRAP to avoid Trace/BPT crashes.
 */

 #define _GNU_SOURCE
 #include <stdio.h>
 #include <stdlib.h>
 #include <unistd.h>
 #include <signal.h>
 #include <sys/wait.h>
 #include <string.h>
 
 /* simple handler that just prints the signal number */
 static void handler(int sig) {
     char buf[64];
     int len = snprintf(buf, sizeof(buf),
                        "    [handler pid=%d] got signal %d\n",
                        getpid(), sig);
     /* write is async‐signal‐safe */
     write(STDOUT_FILENO, buf, len);
 }
 
 void install_handlers(sigset_t *mask_during_handler) {
     int all_sigs[] = {
         SIGINT, SIGQUIT, SIGTSTP,
         SIGABRT, SIGILL,  SIGCHLD,
         SIGSEGV, SIGFPE,  SIGHUP
     };
     struct sigaction sa;
     memset(&sa, 0, sizeof(sa));
     sa.sa_handler = handler;
     sa.sa_mask    = *mask_during_handler;
     sa.sa_flags   = 0;
 
     for (size_t i = 0; i < sizeof(all_sigs)/sizeof(*all_sigs); i++) {
         if (sigaction(all_sigs[i], &sa, NULL) < 0) {
             perror("sigaction");
             exit(1);
         }
     }
 }
 
 void print_pending(const char *tag) {
     sigset_t p;
     sigpending(&p);
     printf(">> %s (pid=%d) pending:", tag, getpid());
     for (int s = 1; s < NSIG; s++) {
         if (sigismember(&p, s))
             printf(" %d", s);
     }
     printf("\n");
 }
 
 int main(void) {
     pid_t pids[4];
 
     /* 0) On macOS, suppress unexpected Trace/BPT traps (SIGTRAP=5) */
     if (signal(SIGTRAP, SIG_IGN) == SIG_ERR) {
         perror("signal(SIGTRAP)");
         /* nonfatal—just continue */
     }
 
     /* 1) Parent permanently blocks INT, QUIT, TSTP */
     sigset_t parent_block;
     sigemptyset(&parent_block);
     sigaddset(&parent_block, SIGINT);
     sigaddset(&parent_block, SIGQUIT);
     sigaddset(&parent_block, SIGTSTP);
     if (sigprocmask(SIG_BLOCK, &parent_block, NULL) < 0) {
         perror("parent sigprocmask");
         exit(1);
     }
 
     /* 2) Install custom handlers so none of the nine signals abort us */
     sigset_t handler_mask;
     sigemptyset(&handler_mask);
     install_handlers(&handler_mask);
 
     print_pending("Parent before fork");
 
     /* 3) Fork four children */
     for (int i = 0; i < 4; i++) {
         if ((pids[i] = fork()) == 0) {
             /* in child */
             sigset_t child_block = parent_block;
             if (i >= 2) {
                 /* second half blocks the other six too */
                 sigaddset(&child_block, SIGABRT);
                 sigaddset(&child_block, SIGILL);
                 sigaddset(&child_block, SIGCHLD);
                 sigaddset(&child_block, SIGSEGV);
                 sigaddset(&child_block, SIGFPE);
                 sigaddset(&child_block, SIGHUP);
             }
             if (sigprocmask(SIG_BLOCK, &child_block, NULL) < 0) {
                 perror("child sigprocmask");
                 exit(1);
             }
 
             /* inherit SIGTRAP‐ignore and install our handlers */
             sigemptyset(&handler_mask);
             install_handlers(&handler_mask);
 
             print_pending(i < 2 ? "Child[inherit]" : "Child[other]");
 
             sleep(10);
             print_pending("Child after sleep");
             exit(0);
         }
     }
 
     /* 4) Parent waits for children to finish */
     for (int i = 0; i < 4; i++) {
         waitpid(pids[i], NULL, 0);
     }
 
     print_pending("Parent after children");
     return 0;
 }
 