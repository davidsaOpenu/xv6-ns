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
#include "obj_fs.h"
#include "obj_file.h"

#include "obj_disk.h"  // for error codes and `new_inode_number`
#include "obj_cache.h"
#include "obj_log.h"


#define min(a, b) ((a) < (b) ? (a) : (b))

//static struct vfs_inode *
//        iget(uint dev, uint inum);

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

struct {
    struct spinlock lock;
    struct obj_inode inode[NINODE];
} obj_icache;

void
obj_iinit(uint dev) {
    initlock(&obj_icache.lock, "obj_icache");
    for(uint i = 0; i < NINODE; i++) {
        initsleeplock(&obj_icache.inode[i].vfs_inode.lock, "obj_inode");
    }
}

void inode_name(char* output, uint inum) {
    const char* prefix = "inode";
    memmove(output, prefix, strlen(prefix));
    for (uint i = 0; i < sizeof(uint) + 1; ++i) {
        output[i + strlen(prefix)] = (inum % 127) + 128;
        inum /= 127;
    }
    output[strlen(prefix) + sizeof(uint) + 1] = 0;  // null terminator
}

//void obj_fsinit(uint dev) {
//    // TODO: to implement
//
//}

//PAGEBREAK!
// Allocate an inode on device dev.
// Mark it as allocated by giving it type `type`.
// Returns an unlocked but allocated and referenced inode.
struct vfs_inode *
obj_ialloc(uint dev, short type) {
    int inum = new_inode_number();
    struct obj_dinode di;
    memset(&di, 0, sizeof(di));
    di.vfs_dinode.type = type;
    di.vfs_dinode.nlink = 0;
    di.data_object_name[0] = 0; //not initialized
    char iname[INODE_NAME_LENGTH];
    inode_name(iname, inum);
    if (log_add_object(&di, sizeof(di), iname) != NO_ERR) {
        panic("ialloc: failed adding inode to disk");
    }
    return obj_iget(dev, inum);
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk, since i-node cache is write-through.
// Caller must hold ip->lock.
void
obj_iupdate(struct vfs_inode *vfs_ip) {
    struct obj_dinode di;
    struct obj_inode * ip = container_of(vfs_ip, struct obj_inode, vfs_inode);

    char iname[INODE_NAME_LENGTH];
    inode_name(iname, ip->vfs_inode.inum);
    di.vfs_dinode.type  = ip->vfs_inode.type;
    di.vfs_dinode.major = ip->vfs_inode.major;
    di.vfs_dinode.minor = ip->vfs_inode.minor;
    di.vfs_dinode.nlink = ip->vfs_inode.nlink;
    memmove(
            di.data_object_name,
            ip->data_object_name,
            MAX_OBJECT_NAME_LENGTH
            );
    if (log_rewrite_object(&di, sizeof(di), iname) != NO_ERR) {
        panic("iupdate: failed writing dinode to the disk");
    }
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
struct vfs_inode *
obj_iget(uint dev, uint inum) {
    struct obj_inode *ip, *empty;

    acquire(&obj_icache.lock);

    // Is the inode already cached?
    empty = 0;
    for (ip = &obj_icache.inode[0]; ip < &obj_icache.inode[NINODE]; ip++) {
        if (ip->vfs_inode.ref > 0 && ip->vfs_inode.dev == dev && ip->vfs_inode.inum == inum) {
            ip->vfs_inode.ref++;
            release(&obj_icache.lock);
            return &ip->vfs_inode;
        }
        if (empty == 0 && ip->vfs_inode.ref == 0)    // Remember empty slot.
            empty = ip;
    }

    // Recycle an inode cache entry.
    if (empty == 0)
        panic("iget: no inodes");

    deviceget(dev);
    ip = empty;
    ip->vfs_inode.dev = dev;
    ip->vfs_inode.inum = inum;
    ip->vfs_inode.ref = 1;
    ip->vfs_inode.valid = 0;
    ip->data_object_name[0] = 0; //not initialized

    /* Initiate inode operations for obj fs */
    ip->vfs_inode.i_op.idup = &obj_idup;
    ip->vfs_inode.i_op.iupdate = &obj_iupdate;
    ip->vfs_inode.i_op.iput = &obj_iput;

    release(&obj_icache.lock);

    return &ip->vfs_inode;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct vfs_inode *
obj_idup(struct vfs_inode *ip) {
    acquire(&obj_icache.lock);
    ip->ref++;
    release(&obj_icache.lock);
    return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void
obj_ilock(struct vfs_inode *vfs_ip) {
    struct dinode di;
    struct obj_inode *ip = container_of(vfs_ip, struct obj_inode, vfs_inode);
    char iname[INODE_NAME_LENGTH];

    if (ip == 0 || ip->vfs_inode.ref < 1)
        panic("obj_ilock");

    acquiresleep(&ip->vfs_inode.lock);

    if (ip->vfs_inode.valid == 0) {
        inode_name(iname, ip->vfs_inode.inum);
        if (cache_get_object(iname, &di) != NO_ERR) {
            panic("inode doesn't exists in the disk");
        }

        ip->vfs_inode.type  = di.vfs_dinode.type;
        ip->vfs_inode.major = di.vfs_dinode.major;
        ip->vfs_inode.minor = di.vfs_dinode.minor;
        ip->vfs_inode.nlink = di.vfs_dinode.nlink;
        ip->vfs_inode.valid = 1;
        if (ip->vfs_inode.type == 0)
            panic("obj_ilock: no type");
    }
}

// Deletes inode and it's content from the disk.
static void
idelete(struct obj_inode *ip)
{
    //log_delete_object panics on failure - no return value check needed.
    if (ip->data_object_name[0] != 0) {
        log_delete_object(ip->data_object_name);
        ip->data_object_name[0] = 0;
    }
    char iname[INODE_NAME_LENGTH];
    inode_name(iname, ip->vfs_inode.inum);
    log_delete_object(iname);
}

// Unlock the given inode.
void
obj_iunlock(struct vfs_inode *ip) {
    if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
        panic("obj_iunlock");

    releasesleep(&ip->lock);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void
obj_iput(struct vfs_inode *vfs_ip) {
    struct obj_inode * ip = container_of(vfs_ip, struct obj_inode, vfs_inode);

    acquiresleep(&ip->vfs_inode.lock);
    if (ip->vfs_inode.valid && ip->vfs_inode.nlink == 0) {
        acquire(&obj_icache.lock);
        int r = ip->vfs_inode.ref;
        release(&obj_icache.lock);
        if (r == 1) {
            // inode has no links and no other references: truncate and free.
            idelete(ip);
            ip->vfs_inode.type = 0;
            ip->vfs_inode.valid = 0;
        }
    }
    releasesleep(&ip->vfs_inode.lock);
    acquire(&obj_icache.lock);

    ip->vfs_inode.ref--;
    if (ip->vfs_inode.ref == 0) {
        deviceput(ip->vfs_inode.dev);
    }
    release(&obj_icache.lock);
}

// Common idiom: unlock, then put.
void
obj_iunlockput(struct vfs_inode *ip) {
    obj_iunlock(ip);
    obj_iput(ip);
}

//PAGEBREAK!
// Inode content

// Copy stat information from inode.
// Caller must hold ip->lock.
void
obj_stati(struct vfs_inode *vfs_ip, struct stat *st) {
    struct obj_inode * ip = container_of(vfs_ip, struct obj_inode, vfs_inode);

    st->dev = ip->vfs_inode.dev;
    st->ino = ip->vfs_inode.inum;
    st->type = ip->vfs_inode.type;
    st->nlink = ip->vfs_inode.nlink;
    if (ip->data_object_name[0] == 0) {
        st->size = 0;
    } else {
        if (cache_object_size(ip->data_object_name, &st->size) != NO_ERR) {
            panic("obj stati failed getting object size");
        }
    }
}

//PAGEBREAK!
// Read data from inode.
// Caller must hold ip->lock.
int
obj_readi(struct vfs_inode *vfs_ip, char *dst, uint off, uint n) {
    struct obj_inode * ip = container_of(vfs_ip, struct obj_inode, vfs_inode);

    if (ip->vfs_inode.type == T_DEV) {
        if (ip->vfs_inode.major < 0 || ip->vfs_inode.major >= NDEV || !devsw[ip->vfs_inode.major].read)
            return -1;
        return devsw[ip->vfs_inode.major].read(vfs_ip, dst, n);
    }

    uint size;
    if (ip->data_object_name[0] == 0) {
        panic("obj_readi reading from inode without data object");
    }
    if (cache_object_size(ip->data_object_name, &size) != NO_ERR) {
        size = 0;
    }
    if(off > size || off + n < off)
        return -1;
    if(off + n > size)
        n = size - off;

    char data[size];
    if (cache_get_object(ip->data_object_name, data) != NO_ERR) {
        panic("readi failed reading object content");
    }
    memmove(dst, data + off, n);
    return n;
}

// PAGEBREAK!
// Write data to inode.
// Caller must hold ip->lock.
int
obj_writei(struct vfs_inode *vfs_ip, char *src, uint off, uint n) {
    struct obj_inode * ip = container_of(vfs_ip, struct obj_inode, vfs_inode);
    uint size = 0;

    if (ip->vfs_inode.type == T_DEV) {
        if (ip->vfs_inode.major < 0 || ip->vfs_inode.major >= NDEV || !devsw[ip->vfs_inode.major].write)
            return -1;
        return devsw[ip->vfs_inode.major].write(vfs_ip, src, n);
    }

    if (ip->data_object_name[0] == 0) {
        panic("obj writei writing to inode without data object");
    }

    if (cache_object_size(ip->data_object_name, &size) != NO_ERR) {
        size = 0;
    }

    if(off > size || off + n < off)
        return -1;
    if(off + n > MAX_INODE_OBJECT_DATA)
        return -1;

    if (size < off + n) {
        size = off + n;
    }
    char data[size];
    if (cache_get_object(ip->data_object_name, data) != NO_ERR) {
        panic("obj_writei failed reading object data");
    }
    memmove(data + off, src, n);
    cache_rewrite_object(data, size, ip->data_object_name);
    return n;
}

//PAGEBREAK!
// Directories

int
obj_namecmp(const char *s, const char *t) {
    return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct vfs_inode *
obj_dirlookup(struct vfs_inode *vfs_dp, char *name, uint *poff) {
    uint off;
    struct dirent de;
    struct obj_inode * dp = container_of(vfs_dp, struct obj_inode, vfs_inode);

    if (dp->vfs_inode.type != T_DIR)
        panic("obj_dirlookup not DIR");

    if (dp->data_object_name[0] == 0) {
        panic("ob_dirlookup received inode without data");
    }
    uint size;
    if (cache_object_size(dp->data_object_name, &size) != NO_ERR) {
        panic("obj_dirlookup failed getting inode data object size");
    }

    for(off = 0; off < size; off += sizeof(de)){
        if (obj_readi(&dp->vfs_inode, (char *) &de, off, sizeof(de)) != sizeof(de))
            panic("obj_dirlookup read");
        if (de.inum == 0)
            continue;
        if (obj_namecmp(name, de.name) == 0) {
            // entry matches path element
            if (poff)
                *poff = off;
            return obj_iget(dp->vfs_inode.dev, de.inum);
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
    struct obj_inode * dp = container_of(vfs_dp, struct obj_inode, vfs_inode);


    // Check that name is not present.
    if ((ip = obj_dirlookup(&dp->vfs_inode, name, 0)) != 0) {
        obj_iput(ip);
        return -1;
    }

    if (dp->data_object_name[0] == 0) {
        panic("obj_dirlink received inode without data");
    }
    uint size;
    if (cache_object_size(dp->data_object_name, &size) != NO_ERR) {
        panic("obj_dirlink failed getting inode data object size");
    }

    // Look for an empty dirent.
    for (off = 0; off < size; off += sizeof(de)) {
        if (obj_readi(&dp->vfs_inode, (char *) &de, off, sizeof(de)) != sizeof(de))
            panic("obj_dirlink read");
        if (de.inum == 0)
            break;
    }

    strncpy(de.name, name, DIRSIZ);
    de.inum = inum;
    if (obj_writei(&dp->vfs_inode, (char *) &de, off, sizeof(de)) != sizeof(de))
        panic("obj_dirlink");

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
    *mnt = getinitialrootmount();

    return obj_iget(ROOTDEV, ROOTINO);
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

    if (*path == '/') {
        curmount = mntdup(getrootmount());
        ip = obj_iget(ROOTDEV, ROOTINO);
    } else {
        curmount = mntdup(myproc()->cwdmount);
        ip = obj_idup(myproc()->cwd);
    }

    while ((path = obj_skipelem(path, name)) != 0) {
        obj_ilock(ip);
        if (ip->type != T_DIR) {
            obj_iunlockput(ip);
            mntput(curmount);
            return 0;
        }
        if (nameiparent && *path == '\0') {
            // Stop one level early.
            obj_iunlock(ip);
            *mnt = curmount;
            return ip;
        }

        if ((next = obj_dirlookup(ip, name, 0)) == 0) {
            obj_iunlockput(ip);
            mntput(curmount);
            return 0;
        }

        obj_iunlockput(ip);
        if ((nextmount = mntlookup(next, curmount)) != 0) {
            mntput(curmount);
            curmount = nextmount;

            obj_iput(next);
            next = obj_iget(curmount->dev, ROOTINO);
        }

        ip = next;
    }
    if (nameiparent) {
        obj_iput(ip);
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

    if (ip != 0) {
        mntput(mnt);
    }

    return ip;
}

struct vfs_inode *
obj_nameiparent(char *path, char *name) {
    struct mount *mnt;
    struct vfs_inode *ip = obj_namex(path, 1, name, &mnt);

    if (ip != 0) {
        mntput(mnt);
    }

    return ip;
}

struct vfs_inode *
obj_nameiparentmount(char *path, char *name, struct mount **mnt) {

    return obj_namex(path, 1, name, mnt);
}

struct vfs_inode *
obj_nameimount(char *path, struct mount **mnt) {

    char name[DIRSIZ];
    return obj_namex(path, 0, name, mnt);
}