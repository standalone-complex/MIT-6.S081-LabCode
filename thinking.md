# Lab9: flie system

## Large files

1. 修改`fs.h`中`struct dinode`字段并且修改、增加宏：

    ```c
    #define NDIRECT 11
    #define NINDIRECT (BSIZE / sizeof(uint))
    #define NDINDIRECT (NINDIRECT * NINDIRECT)
    #define MAXFILE (NDIRECT + NINDIRECT + NDINDIRECT)

    struct dinode {
        short type;           // File type
        short major;          // Major device number (T_DEVICE only)
        short minor;          // Minor device number (T_DEVICE only)
        short nlink;          // Number of links to inode in file system
        uint size;            // Size of file (bytes)
        uint addrs[NDIRECT+2];   // Data block addresses
    };
    ```

2. 修改`file.h`中`struct inode`字段：

    ```c
    struct inode {
        uint dev;           // Device number
        uint inum;          // Inode number
        int ref;            // Reference count
        struct sleeplock lock; // protects everything below here
        int valid;          // inode has been read from disk?

        short type;         // copy of disk inode
        short major;
        short minor;
        short nlink;
        uint size;
        uint addrs[NDIRECT+2];
    };
    ```

3. 在`fs.c`的函数`bmap`中增加字段：

    ```c
    static uint
    bmap(struct inode *ip, uint bn)
    {
        uint addr, *a;
        struct buf *bp;

        if(bn < NDIRECT){
            if((addr = ip->addrs[bn]) == 0)
                ip->addrs[bn] = addr = balloc(ip->dev);
            return addr;
        }
        bn -= NDIRECT;

        if(bn < NINDIRECT){
            // Load indirect block, allocating if necessary.
            if((addr = ip->addrs[NDIRECT]) == 0)
                ip->addrs[NDIRECT] = addr = balloc(ip->dev);
            bp = bread(ip->dev, addr);
            a = (uint*)bp->data;
            if((addr = a[bn]) == 0){
                a[bn] = addr = balloc(ip->dev);
                log_write(bp);
            }
            brelse(bp);
            return addr;
        }
        bn -= NINDIRECT;

        if(bn < NDINDIRECT) {
            if(!(addr = ip->addrs[NDIRECT+1]))
                ip->addrs[NDIRECT+1] = addr = balloc(ip->dev);
            
            bp = bread(ip->dev, addr);
            a = (uint*)bp->data;
            if(!(addr = a[bn/NINDIRECT])) {
                a[bn/NINDIRECT] = addr = balloc(ip->dev);
                log_write(bp);
            }
            brelse(bp);

            bp = bread(ip->dev, addr);
            a = (uint*)bp->data;
            if(!(addr = a[bn%NINDIRECT])) {
                a[bn%NINDIRECT] = addr = balloc(ip->dev);
                log_write(bp);
            }
            brelse(bp);

            return addr;
        }

        panic("bmap: out of range");
    }
    ```

4. 在`fs.c`的函数`itrunc`中增加字段：

    ```c
    void
    itrunc(struct inode *ip)
    {
        int i, j;
        struct buf *bp, *bp2;
        uint *a, *a2;

        for(i = 0; i < NDIRECT; i++){
            if(ip->addrs[i]){
                bfree(ip->dev, ip->addrs[i]);
                ip->addrs[i] = 0;
            }
        }

        if(ip->addrs[NDIRECT]){
            bp = bread(ip->dev, ip->addrs[NDIRECT]);
            a = (uint*)bp->data;
            for(j = 0; j < NINDIRECT; j++){
                if(a[j])
                    bfree(ip->dev, a[j]);
            }
            brelse(bp);
            bfree(ip->dev, ip->addrs[NDIRECT]);
            ip->addrs[NDIRECT] = 0;
        }

        // double indirect inode is exist?
        if(ip->addrs[NDIRECT+1]) {
            bp = bread(ip->dev, ip->addrs[NDIRECT+1]);
            a = (uint*)bp->data;

            for(i = 0; i < NINDIRECT; ++i) {
                // indirect inode is exist?
                if(a[i]) {
                    bp2 = bread(ip->dev, a[i]);
                    a2 = (uint*)bp2->data;

                    for(j = 0; j < NINDIRECT; ++j)
                        // block is exist?
                        if(a2[j])
                            bfree(ip->dev, a2[j]);
                    brelse(bp2);
                    bfree(ip->dev, a[i]);
                }
            }

            brelse(bp);
            bfree(ip->dev, ip->addrs[NDIRECT+1]);
            ip->addrs[NDIRECT+1] = 0;
        }

        ip->size = 0;
        iupdate(ip);
    }
    ```

## Symbolic Links

1. 注册`sys_link`系统调用：

    ```perl
    # usys.pl
    entry("symlink");
    ```

    ```c
    // user.h
    int symlink(char* target, char* path);
    ```

    ```c
    // syscall.h
    #define SYS_symlink 22
    ```

    ```c
    // syscall.c
    extern uint64 sys_symlink(void);

    [SYS_symlink] sys_symlink,
    ```

    ```makefile
    # Makefile
    $U/_symlinktest\
    ```

2. 增加宏，增加`struct inode`字段：

    ```c
    // fcntl.h
    #define O_NOFOLLOW 0x004
    // stat.h
    #define T_SYMLINK 4
    ```

3. 定义函数`sys_symlink`：

    ```c
    uint64 sys_symlink(void) {
        char target[MAXPATH];
        char path[MAXPATH];
        struct inode *ip;

        if(argstr(0, target, MAXPATH) < 0 || argstr(1, path, MAXPATH) < 0)
            return -1;

        begin_op();

        if((ip = create(path, T_SYMLINK, 0, 0)) == 0) {
            end_op();
            return -1;
        }

        if(writei(ip, 0, (uint64)target, 0, MAXPATH) != MAXPATH)
            panic("sys_symlink: writei");
        
        iunlockput(ip);
        end_op();

        return 0;
    }
    ```

4. 修改函数`sys_open`：

    ```c
    uint64
    sys_open(void)
    {
        char path[MAXPATH];
        int fd, omode;
        struct file *f;
        struct inode *ip;
        int n;
        int depth = 0;

        if((n = argstr(0, path, MAXPATH)) < 0 || argint(1, &omode) < 0)
            return -1;

        begin_op();

        if(omode & O_CREATE){
            ip = create(path, T_FILE, 0, 0);
            if(ip == 0){
                end_op();
                return -1;
            }
        } else {
            if((ip = namei(path)) == 0){
                end_op();
                return -1;
            }
            ilock(ip);

            while(ip->type==T_SYMLINK && !(omode&O_NOFOLLOW) && depth<MAXFOLLOWDEPTH) {
                readi(ip, 0, (uint64)path, 0, MAXPATH);
                iunlockput(ip);
                if((ip = namei(path)) == 0) {
                    end_op();
                    return -1;
                }
                ilock(ip);
                ++depth;
            }

            if(depth == MAXFOLLOWDEPTH) {
                iunlock(ip);
                end_op();
                return -1;
            }

            if(ip->type == T_DIR && omode != O_RDONLY){
                iunlockput(ip);
                end_op();
                return -1;
            }
        }

        if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
            iunlockput(ip);
            end_op();
            return -1;
        }

        if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
            if(f)
                fileclose(f);
            iunlockput(ip);
            end_op();
            return -1;
        }

        if(ip->type == T_DEVICE){
            f->type = FD_DEVICE;
            f->major = ip->major;
        } else {
            f->type = FD_INODE;
            f->off = 0;
        }
        f->ip = ip;
        f->readable = !(omode & O_WRONLY);
        f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

        if((omode & O_TRUNC) && ip->type == T_FILE){
            itrunc(ip);
        }

        iunlock(ip);
        end_op();

        return fd;
    }
    ```
