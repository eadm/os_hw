#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/event.h>

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

int add_client() {
    auto pty = create_pty();

    int child = fork();
    if (child == 0) {
        dup2(pty[0], STDIN_FILENO);
        dup2(pty[0], STDOUT_FILENO);
        dup2(pty[0], STDERR_FILENO);

        close(pty[0]);
        close(pty[1]);

        handle_error(execlp("sh", "sh"), "execlp");
        return -1;
    } else {
        close(pty[0]);
        return pty[1];
    }
}

void main_loop(int kq, int socket_fd) {

    const size_t  BUFFER_LEN = 1024;
    char buffer[BUFFER_LEN];

//    printf("Event %d\n", kq);

    struct sockaddr_in client;
    socklen_t client_len;

    struct kevent evSet;
    struct kevent evList[32];
    int events;

    while (1) {
        events = kevent(kq, NULL, 0, evList, 32, NULL);
        if (events < 1) break;

//        printf("events: %d\n", nev);
        for (int i = 0; i < events; i++) {
            if (evList[i].flags & EV_EOF) { // disconnect
                auto fd = evList[i].ident;
                EV_SET(&evSet, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                kevent(kq, &evSet, 1, NULL, 0, NULL);


                printf("Disconnect\n");
            } else if (evList[i].ident == socket_fd) { // connect new one

                printf("New connection\n");
                int client_fd = accept(socket_fd, (sockaddr*) &client, &client_len);

                int pty_fd = add_client();
                int* t1 = new int[3];
                t1[0] = 0;
                t1[1] = pty_fd;
                t1[2] = -1;

                EV_SET(&evSet, client_fd, EVFILT_READ, EV_ADD, 0, 0, (void *)t1);
                kevent(kq, &evSet, 1, NULL, 0, NULL); // add client

                int* t2 = new int[3];
                t2[0] = 1;
                t2[1] = client_fd;
                t2[2] = -1;
                EV_SET(&evSet, pty_fd, EVFILT_READ, EV_ADD, 0, 0, (void *)t2);
                kevent(kq, &evSet, 1, NULL, 0, NULL); // add pty

            } else if (evList[i].flags & EVFILT_READ) {
//                printf("Read %d [%d, %d]\n", (int)evList[i].ident, ((int*)evList[i].udata)[0], ((int*)evList[i].udata)[1]);
                ssize_t x = read((int)evList[i].ident, buffer, BUFFER_LEN);
                write(((int*)evList[i].udata)[1], buffer, x);
//                printf(buffer);
            }
        }
    }
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

    int kq = kqueue();
    handle_error(kq, "kqueue");
    struct kevent evSet;
    int* data = new int[2];
    data[0] = 5;
    data[1] = 5;
    EV_SET(&evSet, socket_fd, EVFILT_READ, EV_ADD, 0, 0, (void*)data);
//    printf("Event %d\n", (int)evSet.ident);
    handle_error(kevent(kq, &evSet, 1, NULL, 0, NULL), "kevent(socket_fd)");

    printf("Waiting on port: %d\n", port);

    main_loop(kq, socket_fd);
    close(socket_fd);
    return 0;
}
