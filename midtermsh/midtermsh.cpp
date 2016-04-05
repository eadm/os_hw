#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

char ** split(char * buff, int n, int *& offsets, char d, int& length, int ending) {
    int size = 10;
    char ** tmp = (char **) malloc (sizeof(char*) * size);
    
    int * offs = (int *) malloc (sizeof(int) * size);
    
    length = 0;
    int i = 0;
    int offset = 0;
    while (i + offset < n) {
        if (buff[i + offset] == d) {
            if (offset == 0) {
                i++;
                continue;
            }
            
            if (length + 1 == size) {
                char** ttt = (char **) malloc (sizeof(char*) * size * 2);
                memcpy(ttt, tmp, size);
                free(tmp);
                tmp = ttt;
                
                int* t_offs = (int *) malloc (sizeof(int) * size * 2);
                memcpy(t_offs, offs, size);
                free(offs);
                offs = t_offs;
                
                size *= 2;
            }
            
            tmp[length] = (char*) malloc (sizeof(char) * offset);
            offs[length] = offset;
            memcpy(tmp[length++], buff + i, offset);
            
            i += offset ;
            offset = 0;
        } else {
            offset++;
        }
    }
    
    if (offset - 1 >= 0) {
        if (length + 1 == size) {
            char** ttt = (char **) malloc (sizeof(char*) * size * 2);
            memcpy(ttt, tmp, size);
            free(tmp);
            tmp = ttt;
            
            int* t_offs = (int *) malloc (sizeof(int) * size * 2);
            memcpy(t_offs, offs, size);
            free(offs);
            offs = t_offs;
        }
        
        tmp[length] = (char*) malloc (sizeof(char) * offset);
        offs[length] = offset;
        memcpy(tmp[length++], buff + i, offset - 1 + ending);
    }
    
    if (ending == 1) {
        tmp[length++] = (char*)0;
    }
    
    offsets = offs;
    return tmp;
}

int pid = 0;
int running = 0;

//int pipes[2];

int execute(char ** commands, int length, int pos) {
    pid = fork();
    if(pid == 0) {
//        if (pos == length - 1) {
//            dup2(pipes[0], STDOUT_FILENO);
//        }
        execvp(commands[0], commands);
        return 0;
    } else {
        running = 1;
        wait(&pid);
        running = 0;
    }
    for (int i = 0; i < length; i++) {
        free(commands[i]);
    }
    
    free(commands);
    return 1;
}

void handler(int s) {
    if (s != SIGINT) return;
    
    if (running == 1) {
        kill(pid, SIGINT);
    } else {
//        exit(0);
    }
}

int main() {
    const int buff_size = 1024;
    
    signal(SIGINT, &handler);
    
//    struct sigaction sa;
//    sa.sa_handler = &handler;
//    sigaction(SIGINT, &sa, NULL);
    
    char* buff = (char*)malloc(sizeof(char) * buff_size);
    
    int r;
    
    write(STDOUT_FILENO, "$ ", 2);
    
    int line_length = 0;
    char* line;
    
    while ((r = read(STDIN_FILENO, buff, buff_size)) != 0) {
        
        if (r == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                break;
            }
        }
        
        
        // reading long commmands
        char* line_t = (char*) malloc (sizeof(char) * (line_length + r));
        memcpy(line_t, line, line_length);
        memcpy(line_t + line_length, buff, r);

        line = line_t;
        line_length += r;
        
        if (buff[r - 1] != '\n') {
            continue;
        }
        
        int pipers_len = 0;
        int *lens;
        
        char ** pipers = split(line, line_length, lens, '|', pipers_len, 0);

        
//        pipe(pipes);
        for (int i = 0; i < pipers_len; i++) {
            int *__ss;
            int length = 0;
            
//            if (i != pipers_len - 1) {
//                dup2(pipes[0], STDOUT_FILENO);
//            } else {
//                dup2(1, STDOUT_FILENO);
//            }
            
            char ** command = split(pipers[i], lens[i], __ss, ' ', length, 1);
            execute(command, length, i);
            free(__ss);
            free(pipers[i]);
            
//            if (i != 0) {
//                dup2(pipes[1], STDIN_FILENO);
//            }
//            dup2(pipes[0], STDOUT_FILENO);
        }
        free(pipers);
        
        
        free(line);
        line_length = 0;
        
        write(STDOUT_FILENO, "$ ", 2);
    }
    free(buff);
    
    return 0;
}