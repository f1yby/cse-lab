#include "inode_manager.h"

#include <deque>
#include <queue>

// disk layer -----------------------------------------

disk::disk() : blocks() { bzero(blocks, sizeof(blocks)); }

void disk::read_block(blockid_t id, char *buf) {
  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void disk::write_block(blockid_t id, const char *buf) {
  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t block_manager::alloc_block() {
  std::unique_lock<std::mutex> l(m_);
  auto buf = static_cast<char *>(malloc(BLOCK_SIZE));
  auto id = 0;
  auto bb = 0;
  for (; id < BLOCK_NUM; ++id) {
    if (bb != BBLOCK(id)) {
      bb = BBLOCK(id);
      read_block(bb, buf);
    }
    if (!(buf[id % BPB / 8] & (1 << (id & 0x7)))) {
      buf[id % BPB / 8] |= (1 << (id & 0x7));
      write_block(bb, buf);
      free(buf);
      return id;
    }
  }

  free(buf);
  return 0;
}

blockid_t block_manager::alloc_block_back() {
  std::unique_lock<std::mutex> l(m_);
  auto buf = static_cast<char *>(malloc(BLOCK_SIZE));
  auto id = BLOCK_NUM - 1;
  auto bb = 0;
  for (; id > 0; --id) {
    if (bb != BBLOCK(id)) {
      bb = BBLOCK(id);
      read_block(bb, buf);
    }
    if (!(buf[id % BPB / 8] & (1 << (id & 0x7)))) {
      buf[id % BPB / 8] |= (1 << (id & 0x7));
      write_block(bb, buf);
      free(buf);
      return id;
    }
  }

  free(buf);
  return 0;
}

void block_manager::free_block(uint32_t id) {
  std::unique_lock<std::mutex> l(m_);
  auto buf = static_cast<char *>(malloc(BLOCK_SIZE));
  auto bb = BBLOCK(id);
  read_block(bb, buf);
  buf[id % BPB / 8] &= ~(1 << (id & 0x7));
  write_block(bb, buf);
  free(buf);
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager() : sb() {
  d = new disk();
  auto blockid_boot = alloc_block();
  if (blockid_boot != 0) {
    printf("\tbm: error! alloc boot block, id %d should be 0\n", blockid_boot);
    exit(1);
  }
  auto blockid_super = alloc_block();
  if (blockid_super != 1) {
    printf("\tbm: error! alloc super block, id %d should be 1\n",
           blockid_super);
    exit(1);
  }
  for (int i = 0; i < BLOCK_NUM / BPB; ++i) {
    alloc_block();
  }
  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;
}

void block_manager::read_block(uint32_t id, char *buf) {
  d->read_block(id, buf);
}
void block_manager::read_block(uint32_t id, char *buf, uint32_t n) {
  if (n >= BLOCK_SIZE) {
    d->read_block(id, buf);
  } else {
    char b[BLOCK_SIZE] = {0};
    d->read_block(id, b);
    memcpy(buf, b, n);
  }
}

void block_manager::write_block(uint32_t id, const char *buf) {
  d->write_block(id, buf);
}

void block_manager::write_block(uint32_t id, const char *buf, uint32_t n) {
  if (n >= BLOCK_SIZE) {
    d->write_block(id, buf);
  } else {
    char b[BLOCK_SIZE] = {0};
    memcpy(b, buf, n);
    d->write_block(id, b);
  }
}

void block_manager::occupy_block(uint32_t id) {
  std::unique_lock<std::mutex> l(m_);
  auto buf = static_cast<char *>(malloc(BLOCK_SIZE));
  auto bb = BBLOCK(id);
  read_block(bb, buf);
  buf[id % BPB / 8] |= (1 << (id & 0x7));
  write_block(bb, buf);
  free(buf);
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
  auto id = bm->alloc_block();
  auto *buf = static_cast<char *>(malloc(BLOCK_SIZE));
  bm->read_block(id, buf);
  auto *i = reinterpret_cast<inode *>(buf);
  i->type = type;
  i->size = 0;
  i->ctime = time(nullptr);
  i->mtime = time(nullptr);
  i->atime = time(nullptr);
  bm->write_block(id, buf);
  free(buf);
  return id - 1 - bm->sb.nblocks / BPB;
}

void inode_manager::free_inode(uint32_t inum) {
  auto bid = IBLOCK(inum, BLOCK_NUM);
  bm->free_block(bid);
}

/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode *inode_manager::get_inode(uint32_t inum) {
  auto buf = static_cast<char *>(malloc(BLOCK_SIZE));
  auto bid = IBLOCK(inum, BLOCK_NUM);
  bm->read_block(BBLOCK(bid), buf);
  if (buf[(bid % BPB) / 8] & (1 << (bid & 0x7))) {
    bm->read_block(bid, buf);
    auto i = reinterpret_cast<inode *>(buf);
    i->atime = time(nullptr);
    bm->write_block(bid, buf);
    return i;
  } else {
    free(buf);
    return nullptr;
  }
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* Get all the data of a file by inum.
 * Return alloced data, should be freed by caller. */
void inode_manager::read_file(uint32_t inum, char **buf_out, uint32_t *size) {
  auto *inode = get_inode(inum);
  if (inode == nullptr) {
    return;
  }
  std::list<uint32_t> queue;
  for (auto i = 0; i < NDIRECT && inode->blocks[i] != 0; ++i) {
    queue.push_back(inode->blocks[i]);
  }
  auto *buf = static_cast<char *>(malloc(BLOCK_SIZE));

  if (queue.size() == NDIRECT) {
    bm->read_block(queue.back(), buf);
    queue.pop_back();
    for (uint32_t i = 0;
         i < NINDIRECT && reinterpret_cast<uint32_t *>(buf)[i] != 0; ++i) {
      queue.push_back(reinterpret_cast<uint32_t *>(buf)[i]);
    }
  }

  *size = inode->size;
  *buf_out = static_cast<char *>(malloc(inode->size));
  uint32_t rsize = 0;
  for (const auto &i : queue) {
    auto s = *size > rsize ? *size - rsize : 0;
    bm->read_block(i, *buf_out + rsize, s);
    rsize += BLOCK_SIZE;
  }
  inode->atime = time(nullptr);
  bm->write_block(IBLOCK(inum, BLOCK_NUM), reinterpret_cast<char *>(inode));
  free(buf);
  free(inode);
}

/* alloc/free blocks if needed */
void inode_manager::write_file(uint32_t inum, const char *buf, uint32_t size) {
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf
   * is larger or smaller than the size of original inode
   */
  auto *inode = get_inode(inum);
  if (inode == nullptr) {
    return;
  }

  std::deque<uint32_t> blocks;
  for (int i = 0; i < NDIRECT && inode->blocks[i] != 0; ++i) {
    blocks.push_back(inode->blocks[i]);
  }
  auto *b = static_cast<char *>(malloc(BLOCK_SIZE));
  if (blocks.size() == NDIRECT) {
    bm->read_block(blocks.back(), b);
    bm->free_block(blocks.back());
    blocks.pop_back();
    for (uint32_t i = 0;
         i < NINDIRECT && reinterpret_cast<uint32_t *>(b)[i] != 0; ++i) {
      if (reinterpret_cast<uint32_t *>(b)[i] < BLOCK_NUM) {
        blocks.push_back(reinterpret_cast<uint32_t *>(b)[i]);
      }
    }
  }

  for (auto i : blocks) {
    bm->free_block(i);
  }
  blocks.clear();
  uint32_t wsize = 0;

  while (wsize < size && blocks.size() < NDIRECT - 1) {
    auto i = bm->alloc_block_back();
    bm->write_block(i, buf + wsize, size - wsize);
    wsize += BLOCK_SIZE;
    blocks.push_back(i);
  }

  auto nb = std::vector<uint32_t>();
  while (wsize < size) {
    auto i = bm->alloc_block_back();
    bm->write_block(i, buf + wsize, size - wsize);
    wsize += BLOCK_SIZE;
    nb.push_back(i);
  }
  if (!nb.empty()) {
    auto i = bm->alloc_block_back();
    bm->write_block(i, reinterpret_cast<const char *>(&nb[0]),
                    sizeof(uint32_t) * nb.size());
    blocks.push_back(i);
  }
  bzero(inode->blocks, NDIRECT * sizeof(blockid_t));
  for (uint32_t i = 0; i < blocks.size(); ++i) {
    inode->blocks[i] = blocks[i];
  }
  inode->size = size;
  inode->ctime = time(nullptr);
  inode->mtime = time(nullptr);
  bm->write_block(IBLOCK(inum, BLOCK_NUM),
                  reinterpret_cast<const char *>(inode));

  free(inode);
  free(b);
}

void inode_manager::get_attr(uint32_t inum, extent_protocol::attr &a) {
  auto *inode = get_inode(inum);
  if (inode == nullptr) {
    return;
  }
  a.type = inode->type;
  a.size = inode->size;
  a.mtime = inode->mtime;
  a.atime = inode->atime;
  a.ctime = inode->ctime;
  free(inode);
}

void inode_manager::remove_file(uint32_t inum) {
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  write_file(inum, nullptr, 0);
  char buf[BLOCK_SIZE] = {0};
  bm->write_block(IBLOCK(inum, BLOCK_NUM), buf);
  free_inode(inum);
}

void inode_manager::occupy_inode(uint32_t inum, uint32_t type) {
  auto id = IBLOCK(inum, BLOCK_NUM);
  bm->occupy_block(id);
  auto *buf = static_cast<char *>(malloc(BLOCK_SIZE));
  bm->read_block(id, buf);
  auto *i = reinterpret_cast<inode *>(buf);
  i->type = type;
  i->size = 0;
  i->ctime = time(nullptr);
  i->mtime = time(nullptr);
  i->atime = time(nullptr);
  bm->write_block(id, buf);
  free(buf);
}
