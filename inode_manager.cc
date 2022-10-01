#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk() : blocks() {
  bzero(blocks, sizeof(blocks));
}

void disk::read_block(blockid_t id, uint8_t *buf) {
  mempcpy(buf, blocks[id], BLOCK_SIZE);
}

void disk::write_block(blockid_t id, const uint8_t *buf) {
  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block() {
  /*
     * your code goes here.
     * note: you should mark the corresponding bit in block bitmap when alloc.
     * you need to think about which block you can start to be allocated.
     */
  auto buf = static_cast<uint8_t *>(malloc(BLOCK_SIZE));
  for (uint32_t bb = 0; bb < BLOCK_NUM / BPB; ++bb) {
    read_block(bb + 2, buf);
    for (uint32_t bo = {}; bo < BLOCK_SIZE; ++bo) {
      for (uint8_t bit = 0; bit < 8; ++bit) {
        if ((buf[bo] & (1 << bit)) == 0) {
          buf[bo] |= 1 << bit;
          write_block(bb + 2, buf);
          free(buf);
          return bb * BPB + bo * 8 + bit;
        }
      }
    }
  }
  free(buf);
  return 0;
}

void block_manager::free_block(uint32_t id) {
  /*
     * your code goes here.
     * note: you should unmark the corresponding bit in the block bitmap when free.
     */
  auto buf = static_cast<uint8_t *>(malloc(BLOCK_SIZE));
  auto bb = BBLOCK(id);
  read_block(id, buf);
  buf[id % BPB] &= (1 << (id & 0x7)) ^ (0xff);
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager() : sb() {
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;
}

void block_manager::read_block(uint32_t id, uint8_t *buf) {
  d->read_block(id, buf);
}

void block_manager::write_block(uint32_t id, const uint8_t *buf) {
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager() {
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t inode_manager::alloc_inode(uint32_t type) {
  /*
     * your code goes here.
     * note: the normal inode block should begin from the 2nd inode block.
     * the 1st is used for root_dir, see inode_manager::inode_manager().
     */
  return 1;
}

void inode_manager::free_inode(uint32_t inum) {
  /*
     * your code goes here.
     * note: you need to check if the inode is already a freed one;
     * if not, clear it, and remember to write back to disk.
     */

  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode *
inode_manager::get_inode(uint32_t inum) {
  struct inode *ino;
  /*
     * your code goes here.
     */

  return ino;
}

void inode_manager::put_inode(uint32_t inum, struct inode *ino) {
  uint8_t buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode *) buf + inum % IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void inode_manager::read_file(uint32_t inum, uint8_t **buf_out, uint32_t *size) {
  /*
     * your code goes here.
     * note: read blocks related to inode number inum,
     * and copy them to buf_out
     */

  return;
}

/* alloc/free blocks if needed */
void inode_manager::write_file(uint32_t inum, const uint8_t *buf, uint32_t size) {
  /*
     * your code goes here.
     * note: write buf to blocks of inode inum.
     * you need to consider the situation when the size of buf
     * is larger or smaller than the size of original inode
     */

  return;
}

void inode_manager::get_attr(uint32_t inum, extent_protocol::attr &a) {
  /*
     * your code goes here.
     * note: get the attributes of inode inum.
     * you can refer to "struct attr" in extent_protocol.h
     */

  return;
}

void inode_manager::remove_file(uint32_t inum) {
  /*
     * your code goes here
     * note: you need to consider about both the data block and inode of the file
     */

  return;
}
