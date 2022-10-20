// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() {
  im = new inode_manager();
  _persister = new chfs_persister("log"); // DO NOT change the dir name here

  // Your code here for Lab2A: recover data on startup
}


int extent_server::create(uint32_t type, extent_protocol::extentid_t &id) {
  // alloc a new inode and return inum
  printf("extent_server: create inode\n");
  id = im->alloc_inode(type);

  return extent_protocol::OK;
}


int extent_server::put(extent_protocol::extentid_t id, std::vector<uint8_t> buf, int &) {
  id &= 0x7fffffff;

  const auto *cbuf = reinterpret_cast<const uint8_t *>(buf.data());
  auto size = buf.size();
  im->write_file(id, cbuf, size);

  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::vector<uint8_t> &buf) {
  printf("extent_server: get %lld\n", id);

  id &= 0x7fffffff;

  uint32_t size = 0;
  uint8_t *cbuf = nullptr;

  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = {};
  else {
    buf.resize(size, 0);
    mempcpy(&buf[0], cbuf, size);
    free(cbuf);
  }

  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a) {
  printf("extent_server: getattr %lld\n", id);

  id &= 0x7fffffff;

  extent_protocol::attr attr;
  memset(&attr, 0, sizeof(attr));
  im->get_attr(id, attr);
  a = attr;

  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &) {
  printf("extent_server: write %lld\n", id);

  id &= 0x7fffffff;
  im->remove_file(id);

  return extent_protocol::OK;
}
