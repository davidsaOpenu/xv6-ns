//// File system implementation.  Five layers:
////   + Blocks: allocator for raw disk blocks.
////   + Log: crash recovery for multi-step updates.
////   + Files: inode allocator, reading, writing, metadata.
////   + Directories: inode with special contents (list of other inodes!)
////   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
////
//// This file contains the low-level file system manipulation
//// routines.  The (higher-level) system call implementations
//// are in sysfile.c.
//
//#include "types.h"
//#include "defs.h"
//#include "param.h"
//#include "stat.h"
//#include "mmu.h"
//#include "proc.h"
//#include "spinlock.h"
//#include "sleeplock.h"
//#include "fs.h"
//#include "buf.h"
//#include "file.h"
//#include "mount.h"
//#include "device.h"
//#include "vfs_fs.h"
//
//// Inodes.
////
//// An inode describes a single unnamed file.
//// The inode disk structure holds metadata: the file's type,
//// its size, the number of links referring to it, and the
//// list of blocks holding the file's content.
////
//// The inodes are laid out sequentially on disk at
//// sb.startinode. Each inode has a number, indicating its
//// position on the disk.
////
//// The kernel keeps a cache of in-use inodes in memory
//// to provide a place for synchronizing access
//// to inodes used by multiple processes. The cached
//// inodes include book-keeping information that is
//// not stored on disk: ip->ref and ip->valid.
////
//// An inode and its in-memory representation go through a
//// sequence of states before they can be used by the
//// rest of the file system code.
////
//// * Allocation: an inode is allocated if its type (on disk)
////   is non-zero. ialloc() allocates, and iput() frees if
////   the reference and link counts have fallen to zero.
////
//// * Referencing in cache: an entry in the inode cache
////   is free if ip->ref is zero. Otherwise ip->ref tracks
////   the number of in-memory pointers to the entry (open
////   files and current directories). iget() finds or
////   creates a cache entry and increments its ref; iput()
////   decrements ref.
////
//// * Valid: the information (type, size, &c) in an inode
////   cache entry is only correct when ip->valid is 1.
////   ilock() reads the inode from
////   the disk and sets ip->valid, while iput() clears
////   ip->valid if ip->ref has fallen to zero.
////
//// * Locked: file system code may only examine and modify
////   the information in an inode and its content if it
////   has first locked the inode.
////
//// Thus a typical sequence is:
////   ip = iget(dev, inum)
////   ilock(ip)
////   ... examine and modify ip->xxx ...
////   iunlock(ip)
////   iput(ip)
////
//// ilock() is separate from iget() so that system calls can
//// get a long-term reference to an inode (as for an open file)
//// and only lock it for short periods (e.g., in read()).
//// The separation also helps avoid deadlock and races during
//// pathname lookup. iget() increments ip->ref so that the inode
//// stays cached and pointers to it remain valid.
////
//// Many internal file system functions expect the caller to
//// have locked the inodes involved; this lets callers create
//// multi-step atomic operations.
////
//// The icache.lock spin-lock protects the allocation of icache
//// entries. Since ip->ref indicates whether an entry is free,
//// and ip->dev and ip->inum indicate which i-node an entry
//// holds, one must hold icache.lock while using any of those fields.
////
//// An ip->lock sleep-lock protects all ip-> fields other than ref,
//// dev, and inum.  One must hold ip->lock in order to
//// read or write that inode's ip->valid, ip->size, ip->type, &c.
//
////struct {
////  struct spinlock lock;
////  struct inode inode[NINODE];
////} icache;
//
//// Increment reference count for ip.
//// Returns ip to enable ip = idup(ip1) idiom.
////struct vfs_inode*
////vfs_idup(struct vfs_inode *ip)
////{
////    cprintf("in vfs_idup before acquire\n");
////    acquire(&icache.lock);
////    cprintf("in vfs_idup after acquire\n");
////
////    ip->ref++;
////    release(&icache.lock);
////    return ip;
////}
//

//void
//vfs_iupdate(struct vfs_inode *ip)
//{
//    struct buf *bp;
//    struct vfs_dinode *dip;
//
//    struct superblock *sb = getsuperblock(ip->dev);
//    bp = bread(ip->dev, IBLOCK(ip->inum, *sb));
//    dip = (struct vfs_dinode*)bp->data + ip->inum%IPB;
//    dip->type = ip->type;
//    dip->major = ip->major;
//    dip->minor = ip->minor;
//    dip->nlink = ip->nlink;
////    dip->size = ip->size;
////    memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
//    log_write(bp);
//    brelse(bp);
//}
//
//// Drop a reference to an in-memory inode.
//// If that was the last reference, the inode cache entry can
//// be recycled.
//// If that was the last reference and the inode has no links
//// to it, free the inode (and its content) on disk.
//// All calls to iput() must be inside a transaction in
//// case it has to free the inode.
//void
//vfs_iput(struct vfs_inode *ip)
//        {
//    acquiresleep(&ip->lock);
//    if(ip->valid && ip->nlink == 0){
//        cprintf("in vfs_iput before acquire\n");
//
//        acquire(&icache.lock);
//        cprintf("in vfs_iput after acquire\n");
//
//        int r = ip->ref;
//        release(&icache.lock);
//        if(r == 1){
//            // inode has no links and no other references: truncate and free.
//            itrunc(ip);
//            ip->type = 0;
//            ip->i_op.iupdate(ip);
//            ip->valid = 0;
//        }
//    }
//    releasesleep(&ip->lock);
//
//    cprintf("in vfs_iput before acquire\n");
//
//    acquire(&icache.lock);
//    cprintf("in vfs_iput after acquire\n");
//
//    ip->ref--;
//    if (ip->ref == 0) {
//        deviceput(ip->dev);
//    }
//    release(&icache.lock);
//}
//
