#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv){
    char buf[4096];
    ssize_t bytes;
    
    while ((bytes = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        
        if (bytes == 1 && buf[0] == '\n') {
            continue; 
        }

        if (bytes >= 2 && (buf[bytes - 2] == '.' || buf[bytes - 2] == ';')) {
            write(STDOUT_FILENO, buf, bytes);
            write(STDERR_FILENO, "OK\n", 3); 
        } else {
            const char msg[] = "Error: string must end with '.' or ';'\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
        }
    }
    return 0;
}