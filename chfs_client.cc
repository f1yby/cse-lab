// chfs client.  implements FS operations using extent and lock server
#include "chfs_client.h"

#include <unistd.h>

#include <iostream>

#include "lock_client.h"

chfs_client::chfs_client(std::string extent_dst, std::string lock_dst) {
  ec = new extent_client(extent_dst);
  lc = new lock_client(lock_dst);
  chfs_command::txid_t txid;
  ec->start_tx(txid);

  acquire(1);
  if (ec->put(1, {}, txid) != extent_protocol::OK) {
    release(1);

    ec->abort_tx(txid);
    return;
  } else {
    release(1);

    ec->commit_tx(txid);
  }
}

bool chfs_client::isfile(inum inum) {
  extent_protocol::attr a{};

  if (ec->getattr(inum, a) != extent_protocol::OK) {
    return false;
  }

  if (a.type == extent_protocol::T_FILE) {
    return true;
  } else {
    return false;
  }
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 *
 * */

bool chfs_client::isdir(inum inum) {
  extent_protocol::attr a{};
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    return false;
  }

  if (a.type == extent_protocol::T_DIR) {
    return true;
  } else {
    return false;
  }
}

bool chfs_client::issymlink(inum inum) {
  extent_protocol::attr a{};
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    return false;
  }

  if (a.type == extent_protocol::T_LINK) {
    return true;
  } else {
    return false;
  }
}

int chfs_client::getfile(inum inum, fileinfo &fin) {
  extent_protocol::attr a{};
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    return IOERR;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;

  return OK;
}

int chfs_client::getdir(inum inum, dirinfo &din) {
  extent_protocol::attr a{};
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    return IOERR;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;
  return OK;
}

// Only support set size of attr
int chfs_client::setattr(inum ino, size_t size) {
  chfs_command::txid_t txid;
  ec->start_tx(txid);

  auto buf = std::string();

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

  ec->create(extent_protocol::T_FILE, txid, ino_out);
  if (ino_out == 0) {
    ec->abort_tx(txid);
    return IOERR;
  }

  auto buf = std::string();
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

int chfs_client::mkdir(inum parent, const char *name, mode_t mode,
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

  ec->create(extent_protocol::T_DIR, txid, ino_out);
  if (ino_out == 0) {
    ec->abort_tx(txid);

    return IOERR;
  }

  auto buf = std::string();
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
  auto buf = std::string();
  ec->get(parent, buf);

  auto l = strlen(name);
  ino_out = 0;
  for (uint32_t i = 0, len; i < buf.size(); i += len) {
    len = buf[i];
    i += 5;
    if (l != len) {
      continue;
    }

    if (memcmp(&buf[i], name, len) == 0) {
      ino_out = *(reinterpret_cast<uint32_t *>(&buf[i - 4]));
      found = true;
      return OK;
    }
  }

  return NOENT;
}

int chfs_client::readdir(inum dir, std::list<dirent> &list) {
  auto buf = std::string();
  ec->get(dir, buf);
  for (uint32_t i = 0, len; i < buf.size(); i += len) {
    len = buf[i];

    i += 1;
    auto ino = *(reinterpret_cast<uint32_t *>(&buf[i]));

    i += 4;
    list.push_back({{buf.begin() + i, buf.begin() + i + len}, ino});
  }

  return OK;
}

int chfs_client::read(inum ino, size_t size, off_t off, std::string &data) {
  auto buf = std::string();

  if (ec->get(ino, buf) != extent_protocol::OK) {
    return IOERR;
  }

  int begin = std::min((int)off, (int)buf.size());
  int end = std::min((int)size + (int)off, (int)buf.size());
  data = std::string(buf.begin() + begin, buf.begin() + end);

  return OK;
}

int chfs_client::write(inum ino, size_t size, off_t off, const char *data,
                       size_t &bytes_written) {
  chfs_command::txid_t txid;
  ec->start_tx(txid);

  auto buf = std::string();
  if (ec->get(ino, buf) != extent_protocol::OK) {
    ec->abort_tx(txid);

    return IOERR;
  }
  if (off + size > buf.size()) {
    buf.resize(off + size, 0);
  }
  bytes_written = size;
  for (uint32_t i = 0; i < size; ++i) {
    buf[off + i] = data[i];
  }

  if (ec->put(ino, buf, txid) != extent_protocol::OK) {
    ec->abort_tx(txid);

    return IOERR;
  }

  ec->commit_tx(txid);

  return OK;
}

int chfs_client::unlink(inum parent, const char *name) {
  chfs_command::txid_t txid;
  ec->start_tx(txid);

  auto buf = std::string();

  ec->get(parent, buf);
  auto l = strlen(name);

  for (uint32_t i = 0, len; i < buf.size(); i += len) {
    len = buf[i];
    i += 5;
    std::cout << len << " "
              << std::string(buf.begin() + i, buf.begin() + i + len)
              << std::endl;
    if (l != len) {
      continue;
    }

    if (memcmp(&buf[i], name, len) == 0) {
      buf.erase(buf.begin() + i - 5, buf.begin() + i + len);

      ec->put(parent, buf, txid);

      ec->commit_tx(txid);

      return OK;
    }
  }

  ec->abort_tx(txid);

  return NOENT;
}

int chfs_client::symlink(chfs_client::inum parent, const char *link,
                         const char *name, chfs_client::inum &ino_out) {
  chfs_command::txid_t txid;
  ec->start_tx(txid);

  ino_out = 0;
  bool exist = false;

  lookup(parent, name, exist, ino_out);
  if (exist) {
    ec->abort_tx(txid);

    return EXIST;
  }

  ec->create(extent_protocol::T_LINK, txid, ino_out);
  if (ino_out == 0) {
    ec->abort_tx(txid);

    return IOERR;
  }

  std::string l(link);
  ec->put(ino_out, {l.begin(), l.end()}, txid);

  auto buf = std::string();
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
  if (ec->get(ino, data) != extent_protocol::OK) {
    return IOERR;
  }
  return OK;
}

void chfs_client::acquire(lock_protocol::lockid_t l) { lc->acquire(l); }
void chfs_client::release(lock_protocol::lockid_t l) { lc->release(l); }
