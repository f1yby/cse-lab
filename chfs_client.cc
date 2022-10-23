// chfs client.  implements FS operations using extent and lock server
#include <fcntl.h>
#include <iostream>
#include <stdio.h>
#include <unistd.h>

#include "chfs_client.h"

/*
 * Your code here for Lab2A:
 * Here we treat each ChFS operation(especially write operation such as 'create',
 * 'write' and 'symlink') as a transaction, your job is to use write ahead log
 * to achive all-or-nothing for these transactions.
 */


chfs_client::chfs_client() { ec = new extent_client(); }


chfs_client::chfs_client(std::string extent_dst, std::string lock_dst) {
  ec = new extent_client();

  chfs_command::txid_t txid;
  ec->start_tx(txid);

  if (ec->put(1, {}, txid) != extent_protocol::OK) {
    ec->abort_tx(txid);
    printf("error init root dir\n");// XYB: init root dir
  }

  ec->commit_tx(txid);
}
chfs_client::inum chfs_client::n2i(std::string n) {
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string chfs_client::filename(inum inum) {
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool chfs_client::isfile(inum inum) {
  extent_protocol::attr a{};

  if (ec->getattr(inum, a) != extent_protocol::OK) {
    printf("error getting attr\n");
    return false;
  }

  if (a.type == extent_protocol::T_FILE) {
    printf("isfile: %lld is a file\n", inum);
    return true;
  }
  printf("isfile: %lld is a dir\n", inum);
  return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool chfs_client::isdir(inum inum) {
  extent_protocol::attr a{};
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    printf("error getting attr\n");
    return false;
  }

  if (a.type == extent_protocol::T_DIR) {
    printf("isfile: %lld is a file\n", inum);
    return true;
  }
  printf("isfile: %lld is a dir\n", inum);
  return false;
}

bool chfs_client::issymlink(inum inum) {
  extent_protocol::attr a{};
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    printf("error getting attr\n");
    return false;
  }

  if (a.type == extent_protocol::T_LINK) {
    printf("isfile: %lld is a file\n", inum);
    return true;
  }
  printf("isfile: %lld is a dir\n", inum);
  return false;
}

int chfs_client::getfile(inum inum, fileinfo &fin) {
  printf("getfile %016llx\n", inum);

  extent_protocol::attr a{};
  if (ec->getattr(inum, a) != extent_protocol::OK) { return IOERR; }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

  return OK;
}

int chfs_client::getdir(inum inum, dirinfo &din) {
  int r = OK;

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a{};
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

release:
  return r;
}


#define EXT_RPC(xx)                                                            \
  do {                                                                         \
    if ((xx) != extent_protocol::OK) {                                         \
      printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__);                   \
      r = IOERR;                                                               \
      goto release;                                                            \
    }                                                                          \
  } while (0)

// Only support set size of attr
// Your code here for Lab2A: add logging to ensure atomicity
int chfs_client::setattr(inum ino, size_t size) {
  chfs_command::txid_t txid;
  ec->start_tx(txid);

  auto buf = std::vector<uint8_t>();

  if (ec->get(ino, buf) != extent_protocol::OK) {
    ec->abort_tx(txid);
    return IOERR;
  }

  buf.resize(size, 0);
  if (ec->put(ino, buf, txid) != extent_protocol::OK) {
    ec->abort_tx(txid);
    return IOERR;
  }

  ec->commit_tx(txid);

  return OK;
}

// Your code here for Lab2A: add logging to ensure atomicity
int chfs_client::create(inum parent, const char *name, mode_t mode,
                        inum &ino_out) {
  chfs_command::txid_t txid;
  ec->start_tx(txid);

  ino_out = 0;
  bool exist = false;

  lookup(parent, name, exist, ino_out);
  if (exist) {

    ec->abort_tx(txid);

    return EXIST;
  }

  ec->create(extent_protocol::T_FILE, ino_out, txid);
  if (ino_out == 0) {
    ec->abort_tx(txid);
    return IOERR;
  }

  auto buf = std::vector<uint8_t>();
  ec->get(parent, buf);

  std::string n(name);
  buf.push_back(n.size());
  buf.resize(buf.size() + 4, 0);

  *(reinterpret_cast<uint32_t *>(&buf[buf.size() - 4])) = ino_out;
  buf.insert(buf.end(), n.begin(), n.end());
  ec->put(parent, buf, txid);

  ec->commit_tx(txid);

  return OK;
}

// Your code here for Lab2A: add logging to ensure atomicity
int chfs_client::mkdir(inum parent, const char *name, mode_t mode,
                       inum &ino_out) {
  chfs_command::txid_t txid;
  ec->start_tx(txid);

  ino_out = 0;
  bool exist = false;

  lookup(parent, name, exist, ino_out);
  if (exist) {
    ec->commit_tx(txid);
    return EXIST;
  }

  ec->create(extent_protocol::T_DIR, ino_out, txid);
  if (ino_out == 0) {
    ec->commit_tx(txid);
    return IOERR;
  }

  auto buf = std::vector<uint8_t>();
  ec->get(parent, buf);
  std::cout << std::string(buf.begin(), buf.end());
  std::string n(name);
  buf.push_back(n.size());
  buf.resize(buf.size() + 4, 0);
  *(reinterpret_cast<uint32_t *>(&buf[buf.size() - 4])) = ino_out;
  buf.insert(buf.end(), n.begin(), n.end());
  ec->put(parent, buf, txid);

  ec->commit_tx(txid);

  return OK;
}

int chfs_client::lookup(inum parent, const char *name, bool &found,
                        inum &ino_out) {
  auto buf = std::vector<uint8_t>();
  ec->get(parent, buf);

  auto l = strlen(name);
  ino_out = 0;
  for (uint32_t i = 0, len = 0; i < buf.size(); i += len) {
    len = buf[i];
    i += 5;
    std::cout << len << " "
              << std::string(buf.begin() + i, buf.begin() + i + len)
              << std::endl;
    if (l != len) { continue; }

    if (memcmp(&buf[i], name, len) == 0) {
      ino_out = *(reinterpret_cast<uint32_t *>(&buf[i - 4]));
      found = true;
      return OK;
    }
  }

  return NOENT;
}

int chfs_client::readdir(inum dir, std::list<dirent> &list) {
  int r = OK;
  auto buf = std::vector<uint8_t>();
  ec->get(dir, buf);
  std::cout << "chfs_client: readdir " << dir << ": buffer_size: " << buf.size()
            << std::endl;
  for (uint32_t i = 0, len = 0; i < buf.size(); i += len) {
    len = buf[i];
    i += 1;
    auto ino = *(reinterpret_cast<uint32_t *>(&buf[i]));
    i += 4;
    std::cout << len << ' '
              << std::string(buf.begin() + i, buf.begin() + i + len)
              << std::endl;
    list.push_back({{buf.begin() + i, buf.begin() + i + len}, ino});
  }


  return r;
}

int chfs_client::read(inum ino, size_t size, off_t off, std::string &data) {
  int r = OK;
  auto buf = std::vector<uint8_t>();

  if (ec->get(ino, buf) != extent_protocol::OK) {
    r = IOERR;
    return r;
  }

  data = std::string(buf.begin() + off, buf.begin() + size + off);

  return r;
}

int chfs_client::write(inum ino, size_t size, off_t off, const char *data,
                       size_t &bytes_written) {
  chfs_command::txid_t txid;
  ec->start_tx(txid);

  int r = OK;
  auto buf = std::vector<uint8_t>();
  if (ec->get(ino, buf) != extent_protocol::OK) {
    r = IOERR;
    return r;
  }
  if (off + size > buf.size()) { buf.resize(off + size, 0); }
  bytes_written = size;
  for (uint32_t i = 0; i < size; ++i) { buf[off + i] = data[i]; }

  if (ec->put(ino, buf, txid) != extent_protocol::OK) {
    r = IOERR;
    return r;
  }

  ec->commit_tx(txid);

  return r;
}

int chfs_client::unlink(inum parent, const char *name) {
  chfs_command::txid_t txid;
  ec->start_tx(txid);

  int r = OK;
  auto buf = std::vector<uint8_t>();

  ec->get(parent, buf);
  auto l = strlen(name);

  for (uint32_t i = 0, len = 0; i < buf.size(); i += len) {
    len = buf[i];
    i += 5;
    std::cout << len << " "
              << std::string(buf.begin() + i, buf.begin() + i + len)
              << std::endl;
    if (l != len) { continue; }

    if (memcmp(&buf[i], name, len) == 0) {
      buf.erase(buf.begin() + i - 5, buf.begin() + i + len);

      ec->put(parent, buf, txid);

      ec->commit_tx(txid);

      return r;
    }
  }
  r = NOENT;

  ec->commit_tx(txid);

  return r;
}

int chfs_client::symlink(chfs_client::inum parent, const char *link,
                         const char *name, chfs_client::inum &ino_out) {
  chfs_command::txid_t txid;
  ec->start_tx(txid);

  ino_out = 0;
  bool exist = false;

  lookup(parent, name, exist, ino_out);
  if (exist) {

    ec->commit_tx(txid);

    return EXIST;
  }

  ec->create(extent_protocol::T_LINK, ino_out, txid);
  if (ino_out == 0) {

    ec->commit_tx(txid);

    return IOERR;
  }

  std::string l(link);
  ec->put(ino_out, {l.begin(), l.end()}, txid);

  auto buf = std::vector<uint8_t>();
  ec->get(parent, buf);

  std::string n(name);
  buf.push_back(n.size());
  buf.resize(buf.size() + 4, 0);
  *(reinterpret_cast<uint32_t *>(&buf[buf.size() - 4])) = ino_out;
  buf.insert(buf.end(), n.begin(), n.end());
  ec->put(parent, buf, txid);

  ec->commit_tx(txid);

  return OK;
}

int chfs_client::readlink(chfs_client::inum ino, std::string &data) {
  auto buf = std::vector<uint8_t>();

  if (ec->get(ino, buf) != extent_protocol::OK) { return IOERR; }
  data = std::string(buf.begin(), buf.end());
  data.push_back(0);
  return OK;
}
