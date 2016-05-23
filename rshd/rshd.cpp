#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>

//using namespace std;

void handle_error(int res, const char* err) {
    if (res == -1) perror(err);
}

int* create_pty() {
    int pty_fd = posix_openpt(O_RDWR);
    handle_error(pty_fd, "posix_openpt");
    handle_error(grantpt(pty_fd), "grantpt");
    handle_error(unlockpt(pty_fd), "unlockpt");

    char *slave = ptsname(pty_fd);
    int slave_fd = open(slave, O_RDWR);
    handle_error(slave_fd, "slave_fd");

    int* tmp = new int[2];
    tmp[0] = slave_fd;
    tmp[1] = pty_fd;

    return tmp;
}

int main(int argc, char** argv) {

    const size_t MAX_CONNECTIONS = 10, BUFFER_LEN = 1024;
    char buffer[BUFFER_LEN];

    if (argc < 2) {
        perror("Port not specified");
	return -1;
    }
	
    int port = atoi(argv[1]);



    socklen_t client_len;
    struct sockaddr_in server, client;
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;


    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (bind(socket_fd, (sockaddr*) &server, sizeof(server)) == -1) {
        perror("Can't bind");
        return -1;
    }

    if (listen(socket_fd, MAX_CONNECTIONS) == -1) {
        perror("Can't listen");
        return -1;
    }



    printf("Waiting on port: %d\n", port);

    int client_fd;
    while ((client_fd = accept(socket_fd, (sockaddr*) &client, &client_len)) != -1) {
        auto pty = create_pty();

        int child = fork();
        if (child == 0) {
            dup2(pty[0], STDIN_FILENO);
            dup2(pty[0], STDOUT_FILENO);
            dup2(pty[0], STDERR_FILENO);

            close(pty[0]);
            close(pty[1]);

            handle_error(execlp("sh", "sh"), "execlp");
        } else {
            fcntl(client_fd, F_SETFL, O_NONBLOCK);
            fcntl(pty[1], F_SETFL, O_NONBLOCK);

            ssize_t i = 0;
            while (1) {
                i = read(client_fd, buffer, BUFFER_LEN);
                if (i != -1) {
                    write(pty[1], buffer, i);
                }
                if (i == 0) break;

                i = read(pty[1], buffer, BUFFER_LEN);
                if (i != -1) {
                    write(client_fd, buffer, i);
                }
                if (i == 0) break;
            }

            close(client_fd);
            close(pty[0]);
            close(pty[1]);
        }
    }


    int state;
    while (wait(&state) > 0) {}

    close(socket_fd);
    return 0;
}
