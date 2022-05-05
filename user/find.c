#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int find(char[], char[]);

int main(int argc, char* argv[]) {
    if(argc != 3) {
        fprintf(2, "usage: find [path] [key]...\n");
    }

    if(find(argv[1], argv[2])) {
        fprintf(2, "find error!\n");
    }

    exit(0);
}

int find(char _path[], char _key[]) {
    int fd;
    struct stat st;
    struct dirent de;
    char buf[512], *p;

    if((fd=open(_path, 0)) < 0) {
        fprintf(2, "ls: open %s error!\n", _path);
        return 1;
    }

    if(fstat(fd, &st) < 0) {
        fprintf(2, "ls: fstat %s error!\n", _path);
        close(fd);
        return 1;
    }

    switch(st.type) {
    case T_FILE:
        fprintf(2, "%s is not a dir!\n", _path);
        close(fd);
        return 0;
    case T_DIR:
        if(strlen(_path)+1+DIRSIZ+1 > sizeof buf) {
            fprintf(2, "ls: path too long!\n");
            return 0;
        }

        strcpy(buf, _path);
        p = buf + strlen(buf);
        *(p++) = '/';

        while(read(fd, &de, sizeof(de)) == sizeof(de)) {
            if(de.inum == 0) {
                continue;
            }

            if(strcmp(de.name, ".")==0 || strcmp(de.name, "..")==0) {
                continue;
            }

            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;

            if(stat(buf, &st) < 0) {
                fprintf(2, "ls: %s stat error", buf);
                continue;
            }

            if(st.type == T_DIR) {
                find(buf, _key);
            } else if(st.type==T_FILE && strcmp(de.name, _key)==0) {
                printf("%s\n", buf);
            }
        }
        break;
    }

    close(fd);

    return 0;
}