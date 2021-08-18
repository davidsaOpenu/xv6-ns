// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"
#include "mount.h"
#include "device.h"
#include "vfs_fs.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

//static struct vfs_inode *
//        iget(uint dev, uint inum);
//static void            itrunc (struct vfs_inode *ip);

// Read the super block.
void
obj_readsb(int dev, struct superblock *sb) {
   // TODO: to implement
}

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// The inodes are laid out sequentially on disk at
// sb.startinode. Each inode has a number, indicating its
// position on the disk.
//
// The kernel keeps a cache of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The cached
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->valid.
//
// An inode and its in-memory representation go through a
// sequence of states before they can be used by the
// rest of the file system code.
//
// * Allocation: an inode is allocated if its type (on disk)
//   is non-zero. ialloc() allocates, and iput() frees if
//   the reference and link counts have fallen to zero.
//
// * Referencing in cache: an entry in the inode cache
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry (open
//   files and current directories). iget() finds or
//   creates a cache entry and increments its ref; iput()
//   decrements ref.
//
// * Valid: the information (type, size, &c) in an inode
//   cache entry is only correct when ip->valid is 1.
//   ilock() reads the inode from
//   the disk and sets ip->valid, while iput() clears
//   ip->valid if ip->ref has fallen to zero.
//
// * Locked: file system code may only examine and modify
//   the information in an inode and its content if it
//   has first locked the inode.
//
// Thus a typical sequence is:
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... examine and modify ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock() is separate from iget() so that system calls can
// get a long-term reference to an inode (as for an open file)
// and only lock it for short periods (e.g., in read()).
// The separation also helps avoid deadlock and races during
// pathname lookup. iget() increments ip->ref so that the inode
// stays cached and pointers to it remain valid.
//
// Many internal file system functions expect the caller to
// have locked the inodes involved; this lets callers create
// multi-step atomic operations.
//
// The icache.lock spin-lock protects the allocation of icache
// entries. Since ip->ref indicates whether an entry is free,
// and ip->dev and ip->inum indicate which i-node an entry
// holds, one must hold icache.lock while using any of those fields.
//
// An ip->lock sleep-lock protects all ip-> fields other than ref,
// dev, and inum.  One must hold ip->lock in order to
// read or write that inode's ip->valid, ip->size, ip->type, &c.

//struct {
//    struct spinlock lock;
//    struct inode inode[NINODE];
//} icache;

void
obj_iinit(uint dev) {
    // TODO: to implement

}

void obj_fsinit(uint dev) {
    // TODO: to implement

}

//PAGEBREAK!
// Allocate an inode on device dev.
// Mark it as allocated by  giving it type type.
// Returns an unlocked but allocated and referenced inode.
//struct vfs_inode *
//obj_ialloc(uint dev, short type) {
//    // TODO: to implement
//
//}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk, since i-node cache is write-through.
// Caller must hold ip->lock.
void
obj_iupdate(struct vfs_inode *vfs_ip) {
    // TODO: to implement

}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
//struct vfs_inode *
//obj_iget(uint dev, uint inum) {
//    ;
//    // TODO: to implement
//    return &f;
//}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct vfs_inode *
obj_idup(struct vfs_inode *ip) {
    // TODO: to implement
    return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void
obj_ilock(struct vfs_inode *vfs_ip) {
    // TODO: to implement

}

// Unlock the given inode.
void
obj_iunlock(struct vfs_inode *ip) {
    // TODO: to implement

}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void
obj_iput(struct vfs_inode *ip) {
    // TODO: to implement

}

// Common idiom: unlock, then put.
void
obj_iunlockput(struct vfs_inode *ip) {
    // TODO: to implement

}

//PAGEBREAK!
// Inode content

// Truncate inode (discard contents).
// Only called when the inode has no links
// to it (no directory entries referring to it)
// and has no in-memory reference to it (is
// not an open file or current directory).
void
obj_itrunc(struct vfs_inode *vfs_ip) {
    // TODO: to implement

}

// Copy stat information from inode.
// Caller must hold ip->lock.
void
obj_stati(struct vfs_inode *vfs_ip, struct stat *st) {
    struct inode * ip = container_of(vfs_ip, struct inode, vfs_inode);

    //cprintf(" in stati\n");

    st->dev = ip->vfs_inode.dev;
    st->ino = ip->vfs_inode.inum;
    st->type = ip->vfs_inode.type;
    st->nlink = ip->vfs_inode.nlink;
    st->size = ip->size;
}

//PAGEBREAK!
// Read data from inode.
// Caller must hold ip->lock.
int
obj_readi(struct vfs_inode *vfs_ip, char *dst, uint off, uint n) {
    // TODO: to implement
    return 0;

}

// PAGEBREAK!
// Write data to inode.
// Caller must hold ip->lock.
int
obj_writei(struct vfs_inode *vfs_ip, char *src, uint off, uint n) {
    // TODO: to implement

   return 0;
}

//PAGEBREAK!
// Directories

int
obj_namecmp(const char *s, const char *t) {
    //cprintf(" in namecmp\n");

    return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct vfs_inode *
obj_dirlookup(struct vfs_inode *vfs_dp, char *name, uint *poff) {
    uint off, inum;
    struct dirent de;
    struct inode * dp = container_of(vfs_dp, struct inode, vfs_inode);

    //cprintf(" in dirlookup\n");

    if (dp->vfs_inode.type != T_DIR)
        panic("dirlookup not DIR");

    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (readi(&dp->vfs_inode, (char *) &de, off, sizeof(de)) != sizeof(de))
            panic("dirlookup read");
        if (de.inum == 0)
            continue;
        if (namecmp(name, de.name) == 0) {
            // entry matches path element
            if (poff)
                *poff = off;
            inum = de.inum;
            return iget(dp->vfs_inode.dev, inum);
        }
    }

    return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
int
obj_dirlink(struct vfs_inode *vfs_dp, char *name, uint inum) {
    int off;
    struct dirent de;
    struct vfs_inode *ip;
    struct inode * dp = container_of(vfs_dp, struct inode, vfs_inode);

    //cprintf(" in dirlink\n");

    // Check that name is not present.
    if ((ip = dirlookup(&dp->vfs_inode, name, 0)) != 0) {
        iput(ip);
        return -1;
    }

    // Look for an empty dirent.
    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (readi(&dp->vfs_inode, (char *) &de, off, sizeof(de)) != sizeof(de))
            panic("dirlink read");
        if (de.inum == 0)
            break;
    }

    strncpy(de.name, name, DIRSIZ);
    de.inum = inum;
    if (writei(&dp->vfs_inode, (char *) &de, off, sizeof(de)) != sizeof(de))
        panic("dirlink");

    return 0;
}

//PAGEBREAK!
// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char *
obj_skipelem(char *path, char *name) {
    char *s;
    int len;

    //cprintf(" in skipelem\n");

    while (*path == '/')
        path++;
    if (*path == 0)
        return 0;
    s = path;
    while (*path != '/' && *path != 0)
        path++;
    len = path - s;
    if (len >= DIRSIZ)
        memmove(name, s, DIRSIZ);
    else {
        memmove(name, s, len);
        name[len] = 0;
    }
    while (*path == '/')
        path++;
    return path;
}

struct vfs_inode *
obj_initprocessroot(struct mount **mnt) {
    //cprintf("before getinitialrootmount\n");
    *mnt = getinitialrootmount();
    //cprintf("after getinitialrootmount\n");

    return iget(ROOTDEV, ROOTINO);
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct vfs_inode *
obj_namex(char *path, int nameiparent, char *name, struct mount **mnt) {
    struct vfs_inode *ip, *next;
    struct mount *curmount;
    struct mount *nextmount;

    //cprintf(" in namex\n");

    if (*path == '/') {
        curmount = mntdup(getrootmount());
        ip = iget(ROOTDEV, ROOTINO);
    } else {
        curmount = mntdup(myproc()->cwdmount);
        ip = idup(myproc()->cwd);
    }

    while ((path = obj_skipelem(path, name)) != 0) {
        ilock(ip);
        if (ip->type != T_DIR) {
            iunlockput(ip);
            mntput(curmount);
            return 0;
        }
        if (nameiparent && *path == '\0') {
            // Stop one level early.
            iunlock(ip);
            *mnt = curmount;
            return ip;
        }

        if ((next = dirlookup(ip, name, 0)) == 0) {
            iunlockput(ip);
            mntput(curmount);
            return 0;
        }

        iunlockput(ip);
        if ((nextmount = mntlookup(next, curmount)) != 0) {
            mntput(curmount);
            curmount = nextmount;

            iput(next);
            next = iget(curmount->dev, ROOTINO);
        }

        ip = next;
    }
    if (nameiparent) {
        iput(ip);
        mntput(curmount);
        return 0;
    }

    *mnt = curmount;
    return ip;
}

struct vfs_inode *
obj_namei(char *path) {
    char name[DIRSIZ];
    struct mount *mnt;
    struct vfs_inode *ip = obj_namex(path, 0, name, &mnt);

    //cprintf(" in namei\n");

    if (ip != 0) {
        mntput(mnt);
    }

    return ip;
}

struct vfs_inode *
obj_nameiparent(char *path, char *name) {
    struct mount *mnt;
    struct vfs_inode *ip = obj_namex(path, 1, name, &mnt);
    //cprintf(" in nameiparent\n");

    if (ip != 0) {
        mntput(mnt);
    }

    return ip;
}

struct vfs_inode *
obj_nameiparentmount(char *path, char *name, struct mount **mnt) {
    //cprintf(" in nameiparentmount\n");

    return obj_namex(path, 1, name, mnt);
}

struct vfs_inode *
obj_nameimount(char *path, struct mount **mnt) {
    //cprintf(" in nameimount\n");

    char name[DIRSIZ];
    return obj_namex(path, 0, name, mnt);
}