#include <stdio.h>
#include <unistd.h>
#include <signal.h>

pid_t pid = 0;
int s = 0;

void handler(int sig, siginfo_t * sinfo, void * v) {
//    printf("CCC");
//    flag = 1;
//	switch(s) {
//		case SIGUSR1:
//			printf("SIGUSR1");
//		break;
//		case SIGUSR2:
//			printf("SIGUSR2");
//		break;
//	}
//    sleep(10);
    s = sig;
    pid = sinfo->si_pid;
}

int main() {
	struct sigaction sa;
		
	sa.sa_sigaction = &handler;
    
    sigset_t block;
    sigemptyset (&block);
    for (int i = 1; i < 32; i++) {
        sigaddset (&block, i);
    }
    
    sa.sa_mask = block;
    sa.sa_flags = 0;
    for (int i = 1; i < 32; i++) {
        sigaction(i, &sa, NULL);
    }

    if (sleep(10) == 0) {
        printf("No signals were caught\n");
    } else {
        printf("%d from %d\n", s, pid);
    }
	return 0;
}	
