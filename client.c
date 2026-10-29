#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

static char SERVER_PROGRAM_NAME[] = "server";

void my_itoa(int num, char *str) {
    int i = 0;
    int is_negative = 0;

    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    if (num < 0) {
        is_negative = 1;
        num = -num;
    }

    while (num != 0) {
        int rem = num % 10;
        str[i++] = rem + '0';
        num = num / 10;
    }

    if (is_negative) {
        str[i++] = '-';
    }

    str[i] = '\0';

    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

int append_str(char *dest, const char *src) {
    int i = 0, j = 0;
    while (dest[i] != '\0') i++;
    while (src[j] != '\0') {
        dest[i] = src[j];
        i++;
        j++;
    }
    dest[i] = '\0';
    return i;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        write(STDERR_FILENO, "usage: ", 7);
        write(STDERR_FILENO, argv[0], strlen(argv[0]));
        write(STDERR_FILENO, " filename\n", 10);
        exit(EXIT_FAILURE);
    }

    char progpath[1024];
    {
        ssize_t len = readlink("/proc/self/exe", progpath, sizeof(progpath) - 1);

        if (len == -1) {
            const char msg[] = "error: failed to read full program path\n";
            write(STDERR_FILENO, msg, sizeof(msg));
            exit(EXIT_FAILURE);
        }

        while (progpath[len] != '/') --len;

        progpath[len] = '\0';
    }

    int file = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (file == -1) {
        const char msg[] = "error: failed to open file\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    int pipe1[2];
    if (pipe(pipe1) == -1) {
        const char msg[] = "error: failed to create a pipe\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }

    int pipe2[2];
    if (pipe(pipe2) == -1) {
        const char msg[] = "error: failed to create a pipe\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }

    const pid_t child = fork();

    switch (child) {
        case -1: {
            const char msg[] = "error: failed to spawn a new process\n";
            write(STDERR_FILENO, msg, sizeof(msg));
            exit(EXIT_FAILURE);
        } break;

        case 0: {
            {
                pid_t pid = getpid();
                char msg[128];
                char num_str[32];
                
                msg[0] = '\0';
                my_itoa(pid, num_str);
                append_str(msg, num_str);
                append_str(msg, ": I'm a child\n");
                
                write(STDOUT_FILENO, msg, strlen(msg));
            }

            close(pipe1[1]);
            close(pipe2[0]);

            dup2(pipe1[0], STDIN_FILENO);
            close(pipe1[0]);

            dup2(file, STDOUT_FILENO);
            close(file);

            dup2(pipe2[1], STDERR_FILENO);
            close(pipe2[1]);

            {
                char path[1024];
                strcpy(path, progpath);
                strcat(path, "/");
                strcat(path, SERVER_PROGRAM_NAME);

                char *const args[] = {SERVER_PROGRAM_NAME, NULL};
                int32_t status = execv(path, args);
                if (status == -1) {
                    const char msg[] = "error: failed to exec into new executable image\n";
                    write(STDERR_FILENO, msg, sizeof(msg) - 1);
                    exit(EXIT_FAILURE);
                }
            }
        } break;

        default: {
            {
                pid_t pid = getpid();
                char msg[128];
                char num_str[32];
                
                msg[0] = '\0';
                
                my_itoa(pid, num_str);
                append_str(msg, num_str);
                append_str(msg, ": I'm a parent, my child has PID ");
                
                my_itoa(child, num_str);
                append_str(msg, num_str);
                append_str(msg, "\n");
                
                write(STDOUT_FILENO, msg, strlen(msg));
            }

            close(pipe1[0]);
            close(pipe2[1]);
            close(file);

            char buf[4096];
            char buf_err[4096];
            ssize_t bytes;

            while ((bytes = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
                if (bytes == 1 && buf[0] == '\n') {
                    break;
                }

                write(pipe1[1], buf, bytes);
                
                int n = read(pipe2[0], buf_err, sizeof(buf_err));
                if (n > 0) {
                    write(STDERR_FILENO, buf_err, n);
                }
            }

            close(pipe1[1]);

            int n;
            while ((n = read(pipe2[0], buf_err, sizeof(buf_err))) > 0) {
                write(STDERR_FILENO, buf_err, n);
            }

            close(pipe2[0]);
            
            wait(NULL);
        } break;
    }
    return 0;
}