#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>

int main(int argc, char* argv[]) {
	const int n = 1024;
	

	int fd = STDIN_FILENO; //(argc > 1) ? open(argv[1], O_RDONLY) : STDIN_FILENO;
    	char buf[n];
	
	int wd = STDOUT_FILENO;
	if (argc > 1) {
		wd = open(argv[1], O_CREAT | O_WRONLY, 0777);
		//close(cd);
		//wd = open(argv[1], O_WRONLY);  
	}

	int i = 0;
    	while ((i = read(fd, buf, n)) != 0) {
		if (i == -1) {
			if (errno == EINTR) {
				perror("Ignoring eintr on reading");
				continue;
			} else {
				perror("Reading error");
				return 0;
			}	
		}	

		int offset = 0;
		while (offset < i) {
			int w = write(wd, buf + offset, i - offset);
			
			if (w == -1) {		
				if (errno == EINTR) {
 					perror("Ignoring eintr of writing");
					continue;
				} else {
					perror("Writing error");
					return 0;
				}	
			}
			offset += w;
		}	
        }
	
	if(argc > 1) close(wd);
    	return 0;
}
