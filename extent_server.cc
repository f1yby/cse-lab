#include "extent_server.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <sstream>

    extent_server::extent_server() {
  im = new inode_manager();
}

int extent_server::create(uint32_t type, extent_protocol::extentid_t &id) {
  // alloc a new inode and return inum
  printf("extent_server: create inode\n");
  id = im->alloc_inode(type);

  return extent_protocol::OK;
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &) {
  id &= 0x7fffffff;

  auto size = buf.size();
  im->write_file(id, buf.c_str(), size);

  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf) {
  id &= 0x7fffffff;

  uint32_t size = 0;
  char *cbuf = nullptr;

  im->read_file(id, &cbuf, &size);
  if (size == 0) {
    buf = {};
  } else {
    buf.resize(size, 0);
    mempcpy(&buf[0], cbuf, size);
    free(cbuf);
  }

  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id,
                           extent_protocol::attr &a) {
  id &= 0x7fffffff;

  extent_protocol::attr attr{};
  memset(&attr, 0, sizeof(attr));
  im->get_attr(id, attr);
  a = attr;

  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &) {
  id &= 0x7fffffff;
  im->remove_file(id);

  return extent_protocol::OK;
}
