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
#include <signal.h>

//using namespace std;

const bool echo = false;

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
    size_t offset, len;

    smart_buffer(size_t len): BUFFER_LEN(len), offset(0), len(0) {
        buff = new char[len];
    }

    void reset() {
        offset = 0;
        len = 0;
    }

    void set(const char* data, size_t length) {
        memcpy(buff, data, length);
        offset = 0;
        len = length;
    }

    ~smart_buffer() {
        delete buff;
    }
};

class rshd {
public:
    rshd(int socket): kq(kqueue()), socket_fd(socket) {
        add_listener(socket_fd, EVFILT_READ);
    }

    void start() {
        while (1) {
            events = kevent(kq, NULL, 0, evList, 32, NULL);
            if (events < 1) break;

            for (int i = 0; i < events; i++) {
                int fd = (int)evList[i].ident;

                if (evList[i].flags & EV_EOF) {
                    if (!pty_map[fd]){
                        buffers[destinations[fd]]->set("exit\n", 5); // send exit to tty

                        change_listener(destinations[fd], EVFILT_WRITE, EV_ENABLE);
                        remove_listener(fd, EVFILT_READ);
                    }
                } else if (fd == socket_fd) { // connect new one
                    int client_fd = accept(socket_fd, (sockaddr*) &client, &client_len);
                    int pty_fd = add_client(pid_map);

                    pty_map[pty_fd] = true;
                    pty_map[client_fd] = false;

                    connect(client_fd, pty_fd);
                } else if (evList[i].filter == EVFILT_WRITE) {
                    write_fd(fd);
                } else if (evList[i].filter == EVFILT_READ) {
                    read_fd(fd);
                }
            }
        }
    }
private:
    int kq, socket_fd;

    void read_fd(int fd) {
        ssize_t x = read(fd, buffers[destinations[fd]]->buff, BUFFER_LEN); // read to target buffer
        if (x > 0) {
            buffers[destinations[fd]]->offset = 0;
            buffers[destinations[fd]]->len = (size_t)x;

            change_listener(destinations[fd], EVFILT_WRITE, EV_ENABLE);
            change_listener(fd, EVFILT_READ, EV_DISABLE);
        }

        if (x == 0 && pty_map[fd]) {
            int fd1 = fd;
            int fd2 = destinations[fd1];

            disconnect(fd1, fd2);
        }
    }

    void write_fd(int fd) {
        ssize_t x = write(fd, buffers[fd]->buff + buffers[fd]->offset, buffers[fd]->len);
        if (x != -1) {
            if (x == buffers[fd]->len) {
                buffers[fd]->reset();

                change_listener(destinations[fd], EVFILT_READ, EV_ENABLE);
                change_listener(fd, EVFILT_WRITE, EV_DISABLE);
            } else {
                buffers[fd]->len -= x;
                buffers[fd]->offset += x;
            }
        }
    }

    void disconnect(int fd1, int fd2) {
        unreg(fd1);
        unreg(fd2);

        close(fd1);
        close(fd2);
        if (echo) printf("Disconnect %d %d\n", fd1, fd2);
    }

    void unreg(int fd) {
        remove_listener(fd, EVFILT_READ);
        remove_listener(fd, EVFILT_WRITE);

        if (pid_map.count(fd) != 0) {
            kill(pid_map[fd], SIGTERM);
            waitpid(pid_map[fd], 0, 0); // wait for zombie
            pid_map.erase(fd);
        }

        delete buffers[fd];
        buffers.erase(fd);
        destinations.erase(fd);
        pty_map.erase(fd);
    }

    void connect(int client_fd, int pty_fd) {
        reg(client_fd, pty_fd);
        reg(pty_fd, client_fd);

        if (echo) printf("New connection pty[%d] client[%d]\n", pty_fd, client_fd);
    }

    void reg(int from, int to) {
        smart_buffer* buf = new smart_buffer(BUFFER_LEN);
        buffers[to] = buf;
        destinations[from] = to;

        add_listener(from, EVFILT_READ);
        add_listener(from, EVFILT_WRITE);
        change_listener(from, EVFILT_WRITE, EV_DISABLE);
    }


    int change_listener(int fd, int type, int state) {
        EV_SET(&evSet, fd, type, state, 0, 0, NULL);
        return kevent(kq, &evSet, 1, NULL, 0, NULL);
    }

    int add_listener(int fd, int type) {
        EV_SET(&evSet, fd, type, EV_ADD, 0, 0, NULL);
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

    std::map<int, bool> pty_map;
};


int main(int argc, char** argv) {

    if (!echo) make_daemon();
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

    if (echo) printf("Waiting on port: %d\n", port);

    rshd rshd_server(socket_fd);
    rshd_server.start();
    close(socket_fd);
    return 0;
}