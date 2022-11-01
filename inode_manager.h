// inode layer interface.

#ifndef inode_h
#define inode_h

#include <stdint.h>

#include "extent_protocol.h"

#define DISK_SIZE (1024 * 1024 * 16)
#define BLOCK_SIZE 512
#define BLOCK_NUM (DISK_SIZE / BLOCK_SIZE)

typedef uint32_t blockid_t;

// disk layer -----------------------------------------

class disk {
 private:
  unsigned char blocks[BLOCK_NUM][BLOCK_SIZE];

 public:
  disk();
  void read_block(uint32_t id, uint8_t *buf);
  void write_block(uint32_t id, const uint8_t *buf);
};

// block layer -----------------------------------------

typedef struct superblock {
  uint32_t size;
  uint32_t nblocks;
  uint32_t ninodes;
} superblock_t;

class block_manager {
 private:
  disk *d;
  std::map<uint32_t, int> using_blocks;

 public:
  block_manager();
  struct superblock sb;

  uint32_t alloc_block();
  uint32_t alloc_block_back();
  void occupy_block(uint32_t id);
  void free_block(uint32_t id);
  void read_block(uint32_t id, uint8_t *buf);
  void read_block(uint32_t id, uint8_t *buf, uint32_t n);
  void write_block(uint32_t id, const uint8_t *buf);
  void write_block(uint32_t id, const uint8_t *buf, uint32_t n);
};

// inode layer -----------------------------------------

#define INODE_NUM 1024

// Inodes per block.
#define IPB 1
//(BLOCK_SIZE / sizeof(struct inode))

// Block containing inode i
#define IBLOCK(i, nblocks) ((nblocks) / BPB + (i - 1) / IPB + 2)

// Bitmap bits per block
#define BPB (BLOCK_SIZE * 8)

// Block containing bit for block b
#define BBLOCK(b) ((b) / BPB + 2)

#define NDIRECT 123
#define NINDIRECT (BLOCK_SIZE / sizeof(uint32_t))
#define MAXFILE (NDIRECT + NINDIRECT)

typedef struct inode {
  unsigned int type;
  unsigned int size;
  unsigned int atime;
  unsigned int mtime;
  unsigned int ctime;

  blockid_t blocks[NDIRECT];  // Data block addresses
} inode_t;

class inode_manager {
 private:
  block_manager *bm;
  struct inode *get_inode(uint32_t inum);

 public:
  inode_manager();
  uint32_t alloc_inode(uint32_t type);
  void occupy_inode(uint32_t inum, uint32_t type);
  void free_inode(uint32_t inum);
  void read_file(uint32_t inum, uint8_t **buf, uint32_t *size);
  void write_file(uint32_t inum, const uint8_t *buf, uint32_t size);
  void remove_file(uint32_t inum);
  void get_attr(uint32_t inum, extent_protocol::attr &a);
};

#endif
