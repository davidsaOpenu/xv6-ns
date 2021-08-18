//
// File descriptors
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "obj_fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "obj_file.h"

//PAGEBREAK!
// Write to file f.
int
obj_filewrite(struct vfs_file *f, char *addr, int n) {
    int r;

    if (f->writable == 0)
        return -1;
    if (f->type == FD_PIPE)
        return pipewrite(f->pipe, addr, n);
    if (f->type == FD_INODE) {
        // write a few blocks at a time to avoid exceeding
        // the maximum log transaction size, including
        // i-node, indirect block, allocation blocks,
        // and 2 blocks of slop for non-aligned writes.
        // this really belongs lower down, since writei()
        // might be writing a device like the console.
        int max = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * 512;
        int i = 0;
        while (i < n) {
            int n1 = n - i;
            if (n1 > max)
                n1 = max;

            obj_ilock(f->ip);
            if ((r = obj_writei(f->ip, addr + i, f->off, n1)) > 0)
                f->off += r;
            obj_iunlock(f->ip);

            if (r < 0)
                break;
            if (r != n1)
                panic("short obj filewrite");
            i += r;
        }
        return i == n ? n : -1;
    }
    panic("obj_filewrite");
}

