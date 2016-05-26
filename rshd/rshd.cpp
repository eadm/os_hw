#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/event.h>
#include <vector>
#include <map>

//using namespace std;

void handle_error(int res, const char* err) {
    if (res == -1) perror(err);
}

void make_daemon() {
    auto child = fork();
    handle_error(child, "fork");
    if (child != 0) {
        exit(0);
    } else {
        setsid();
        child = fork();

        if (child == 0) {
            return;
        } else {
            exit(0);
        }
    }
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

int add_client(std::map<int, pid_t>& pid_map) {
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
        pid_map[pty[1]] = child;
        return pty[1];
    }
}

struct smart_buffer {
    char* buff;
    size_t BUFFER_LEN;

    bool filled = false, listened = false;
    size_t offset, len;

    smart_buffer(size_t len): BUFFER_LEN(len), offset(0), len(0) {
        buff = new char[len];
    }

    void reset() {
        offset = 0;
        len = 0;
        filled = false;
    }

    ~smart_buffer() {
        delete buff;
    }
};

class rshd {
public:
    rshd(int socket): socket_fd(socket) {
        kq = kqueue();
        add_listener(socket_fd, EVFILT_READ, NULL);
    }

    void start() {
        while (1) {
            events = kevent(kq, NULL, 0, evList, 32, NULL);
            if (events < 1) break;

            for (int i = 0; i < events; i++) {
                if (evList[i].flags & EV_EOF) {
                    int fd1 = (int)evList[i].ident;
                    int fd2 = destinations[fd1];
                    disconnect(fd1, fd2);
                } else if (evList[i].ident == socket_fd) { // connect new one
                    int client_fd = accept(socket_fd, (sockaddr*) &client, &client_len);
                    int pty_fd = add_client(pid_map);

                    connect(client_fd, pty_fd);
                } else if (evList[i].flags & EVFILT_READ) {
                    int* data = (int*)evList[i].udata;
                    int fd = (int)evList[i].ident;

                    if (data[0] == 3) {
                        write_fd(fd);
                    } else {
                        read_fd(fd);
                    }
                }

            }

        }
    }
private:
    int kq, socket_fd;

    void read_fd(int fd) {
        if (buffers[destinations[fd]]->filled) { // if targeted buffer filled wait for it
            return;
        }

        ssize_t x = read(fd, buffers[destinations[fd]]->buff, BUFFER_LEN); // read to target buffer
        if (x != -1) {
            buffers[destinations[fd]]->offset = 0;
            buffers[destinations[fd]]->len = (size_t)x;
            buffers[destinations[fd]]->filled = true;
        }

        if (!buffers[destinations[fd]]->listened) {
            int* data = new int[1];
            data[0] = 3; //state

            add_listener(destinations[fd], EVFILT_WRITE, data);
            buffers[destinations[fd]]->listened = true;
        }
    }

    void write_fd(int fd) {
        if (!buffers[fd]->filled) {
            return;
        }
        ssize_t x = write(fd, buffers[fd]->buff + buffers[fd]->offset, buffers[fd]->len);
        if (x != -1) {
            if (x == buffers[fd]->len) {
                buffers[fd]->reset();
                remove_listener(fd, EVFILT_WRITE);
                buffers[fd]->listened = false;
            } else {
                buffers[fd]->len -= x;
                buffers[fd]->offset += x;
            }
        }
    }

    void disconnect(int fd1, int fd2) {
        unreg(fd1);
        unreg(fd2);

        printf("Disconnect %d %d\n", fd1, fd2);
    }

    void unreg(int fd) {
        remove_listener(fd, EVFILT_READ);
        remove_listener(fd, EVFILT_WRITE);

        delete buffers[fd];
        buffers.erase(fd);
        destinations.erase(fd);
    }

    void connect(int client_fd, int pty_fd) {
        reg(client_fd, pty_fd);
        reg(pty_fd, client_fd);

        printf("New connection pty[%d] client[%d]\n", pty_fd, client_fd);
    }

    void reg(int from, int to) {
        smart_buffer* buf = new smart_buffer(BUFFER_LEN);
        buffers[to] = buf;
        destinations[from] = to;

        int* data = new int[1];
        data[0] = 1;
        add_listener(from, EVFILT_READ, data);
    }

    int add_listener(int fd, int type, void* data) {
        EV_SET(&evSet, fd, type, EV_ADD, 0, 0, data);
        return kevent(kq, &evSet, 1, NULL, 0, NULL);
    }

    int remove_listener(int fd, int type) {
        EV_SET(&evSet, fd, type, EV_DELETE, 0, 0, NULL);
        return kevent(kq, &evSet, 1, NULL, 0, NULL);
    }

    const size_t BUFFER_LEN = 1024;

    struct sockaddr_in client;
    socklen_t client_len;

    struct kevent evSet;
    struct kevent evList[32];

    int events;

    std::map<int, int> destinations; // pty to socket, socket to pty
    std::map<int, smart_buffer*> buffers; // fd to buffer from its write

    std::map<int, pid_t> pid_map;
};


int main(int argc, char** argv) {

    make_daemon();
    const size_t MAX_CONNECTIONS = 10;

    if (argc < 2) {
        perror("Port not specified");
	return -1;
    }
	
    int port = atoi(argv[1]);



    struct sockaddr_in server;
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

    rshd rshd_server(socket_fd);
    rshd_server.start();
    close(socket_fd);
    return 0;
}
