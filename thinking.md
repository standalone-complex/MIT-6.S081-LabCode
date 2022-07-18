# Lab9: flie system

## Large files

1. 修改fs.h中dinode结构体字段并且修改、增加宏：

    ```c
    #define NDIRECT 11
    #define NINDIRECT (BSIZE / sizeof(uint))
    #define NSECINDIRECT (NINDIRECT * NINDIRECT)
    #define MAXFILE (NDIRECT + NINDIRECT + NSECINDIRECT)

    struct dinode {
        short type;           // File type
        short major;          // Major device number (T_DEVICE only)
        short minor;          // Minor device number (T_DEVICE only)
        short nlink;          // Number of links to inode in file system
        uint size;            // Size of file (bytes)
        uint addrs[NDIRECT+2];   // Data block addresses
    };
    ```

2. 修改file.h中inode结构体字段：

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

3. 在fs.c的函数bmap中增加字段：

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

        if(bn < NSECINDIRECT) {
            if((addr = ip->addrs[NDIRECT+1]) == 0)
                ip->addrs[NDIRECT+1] = addr = balloc(ip->dev);
            bp = bread(ip->dev, addr);
            a = (uint*)bp->data;

            if((addr = a[bn/NINDIRECT]) == 0) {
                a[bn/NINDIRECT] = addr = balloc(ip->dev);
                log_write(bp);
            }
            brelse(bp);

            bp = bread(ip->dev, addr);
            a = (uint*)bp->data;

            if((addr = a[bn%NINDIRECT]) == 0) {
                a[bn%NINDIRECT] = addr = balloc(ip->dev);
                log_write(bp);
            }
            brelse(bp);

            return addr;
        }

        panic("bmap: out of range");
    }
    ```

4. 在fs.c的函数itrunc中增加字段：

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

    if(ip->addrs[NDIRECT+1]) {
        bp = bread(ip->dev, ip->addrs[NDIRECT+1]);
        a = (uint*)bp->data;

        for(i = 0; i < NINDIRECT; ++i) {
            if(a[i]) {
                bp2 = bread(ip->dev, a[i]);
                a2 = (uint*)bp->data;

                for(j = 0; j < NINDIRECT; ++j)
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
