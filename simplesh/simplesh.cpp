#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <iostream>
#include <vector>

using namespace std;

int pid = 0;
bool running = false;

void handler(int s) {
    if (s != SIGINT) return;
    if (running) {
        kill(pid, SIGINT);
    } else {
        exit(0);
    }
}

char* append(char * next, char * buff, ssize_t r) {
    size_t next_len = (next == NULL) ? 1 : strlen(next) + 1;
    size_t tmp_len = next_len + r;
    char* tmp = (char*) malloc (sizeof(char) * tmp_len);
    tmp[0] = '\0'; // говорим C, что начало строки в начале строки


    if (next != NULL && next_len > 1) { // тк если next_len == 1, то там только \n
        strcat(tmp, next);
    }
    if (r > 0) strcat(tmp, buff);
    return tmp;
}

char* get_current_line(char*& next, char* buff, ssize_t r) {
    char* tmp = append(next, buff, r);
    next = strchr(tmp, '\n');

    if (next != NULL) { // помечаем конец строки
        tmp[next - tmp] = '\0';
        next++;
    }

    return tmp;
}

vector<char*> split(char* line, char d) {
    vector<char*> result;
    char* next;

    while ((next = strchr(line, d)) != NULL) {
        line[next - line] = '\0';
        if (next - line > 0) result.push_back(line);
        line = next + 1;
    }
    if (strlen(line) > 0) result.push_back(line);

    return result;
}

int pipes1[2];
int pipes2[2];

int execute(char ** commands, bool isFirst, bool isLast) {
    pipe(pipes2);
    pid = fork();
    if(pid == 0) {
        if (!isFirst) {
            dup2(pipes1[0], 0);
            close(pipes1[0]);
            close(pipes1[1]);
        }
        if (!isLast) {
            dup2(pipes2[1], 1);
        }
        close(pipes2[0]);
        close(pipes2[1]);
        execvp(commands[0], commands);
        return 0;
    } else {
        if (!isFirst) {
            close(pipes1[0]);
            close(pipes1[1]);
        }
        if (isLast) {
            close(pipes2[0]);
            close(pipes2[1]);
        }
    }
    pipes1[0] = pipes2[0];
    pipes1[1] = pipes2[1];
    return 1;
}

void execute_line(char * line) {
    vector<char *> pipers = split(line, '|');
    size_t count = 0;
    for (auto c : pipers) {
        vector<char*> commands = split(c, ' ');
        commands.push_back(NULL); // покажем C, что массив закончился
        execute(commands.data(), count == 0, count == pipers.size() - 1);
        count++;
    }
    running = true;
    int stat;
    while ((pid = wait(&stat)) > 0); // ждем пока все закончится
    running = false;
}

int main() {
    const int buff_size = 1024;
//    const char* welcome = "\x1B[35m$\x1B[0m ";
    const char* welcome = "$ ";
    signal(SIGINT, &handler);

    char* buff = (char*)malloc(sizeof(char) * buff_size);
    write(STDOUT_FILENO, welcome, strlen(welcome));
    
    ssize_t r;
    char* next = NULL;
    
    while ((r = read(STDIN_FILENO, buff, buff_size)) != 0) {
        if (r == -1) {
            if (errno == EINTR) {
                perror("EINTR caught\n");
                continue;
            } else {
                return 1;
            }
        }
        if (buff[r - 1] != '\n') {
            next = append(next, buff, r);
            continue;
        }

        buff[r] = '\0';
        execute_line(get_current_line(next, buff, r));
        write(STDOUT_FILENO, welcome, strlen(welcome));
    }

    while (next != NULL && strlen(next) > 1) {
        execute_line(get_current_line(next, buff, 0));
        write(STDOUT_FILENO, welcome, strlen(welcome));
    }

    free(buff);
    return 0;
}