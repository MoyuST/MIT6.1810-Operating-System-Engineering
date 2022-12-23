// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

struct buf buckets[NBUC];
struct spinlock bucketlock[NBUC];

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for(int i=0; i<NBUC; ++i){
    initlock(&bucketlock[i], "bcache.bucket");
    buckets[i].prev = &buckets[i];
    buckets[i].next = &buckets[i];
  }

  // chain all free mem to bucket 0
  for(int i=0; i < NBUF; ++i){
    b = &bcache.buf[i];
    b->next = buckets[0].next;
    b->prev = &buckets[0];
    initsleeplock(&b->lock, "buffer");
    buckets[0].next->prev = b;
    buckets[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int hashidx = blockno%NBUC;

  acquire(&bucketlock[hashidx]);

  // Is the block already cached?
  for(b = buckets[hashidx].next; b != &buckets[hashidx]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bucketlock[hashidx]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bucketlock[hashidx]);

  // Not cached.
  for(int i = 0; i<NBUC ; i++){
    int find = 0;
    acquire(&bucketlock[i]);

    for(b = buckets[i].next; b != &buckets[i]; b = b->next){
      if(b->refcnt == 0){
        find = 1;

        b->next->prev = b->prev;
        b->prev->next = b->next;
      }

      if(find){
        break;
      }
    } 

    release(&bucketlock[i]);

    if(find){
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;

      acquire(&bucketlock[hashidx]);

      b->next = buckets[hashidx].next;
      b->prev = &buckets[hashidx];

      buckets[hashidx].next->prev = b;
      buckets[hashidx].next = b;

      release(&bucketlock[hashidx]);

      acquiresleep(&b->lock);

      return b;
    }
  }  
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  // since no need to consider 2 process using the same block
  int hashidx = b->blockno%NBUC;
  int modify=0;

  while(!modify){
    acquire(&bucketlock[hashidx]);
    int curhash = b->blockno % NBUC;
    if(curhash == hashidx){
      b->refcnt--;
      modify = 1;
    }
    release(&bucketlock[hashidx]);
    hashidx = curhash;
  }


}

void
bpin(struct buf *b) {
  int hashidx = b->blockno%NBUC;
  int modify=0;

  while(!modify){
    acquire(&bucketlock[hashidx]);
    int curhash = b->blockno % NBUC;
    if(curhash == hashidx){
      b->refcnt++;
      modify = 1;
    }
    release(&bucketlock[hashidx]);
    hashidx = curhash;
  }
}

void
bunpin(struct buf *b) {
  int hashidx = b->blockno%NBUC;
  int modify=0;

  while(!modify){
    acquire(&bucketlock[hashidx]);
    int curhash = b->blockno % NBUC;
    if(curhash == hashidx){
      b->refcnt--;
      modify = 1;
    }
    release(&bucketlock[hashidx]);
    hashidx = curhash;
  }
}


