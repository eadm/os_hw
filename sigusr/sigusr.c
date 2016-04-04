#include <stdio.h>
#include <unistd.h>
#include <signal.h>

void handler(int s, struct __siginfo * sinfo, void * v) {
	switch(s) {
		case SIGUSR1:
			printf("SIGUSR1");
		break;
		case SIGUSR2:
			printf("SIGUSR2");
		break;
	}
	printf(" from %d\n", sinfo->si_pid);
}

int main() {
	struct sigaction sa;
		
	sa.sa_sigaction = &handler;
	sigaction(SIGUSR1, &sa, NULL);	
	sigaction(SIGUSR2, &sa, NULL);	

	if (sleep(10) == 0) printf("No signals were caught\n");
	return 0;
}	
