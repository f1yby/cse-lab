// the extent server implementation

#include "extent_server.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <sstream>

#include "persister.h"

extent_server::extent_server() : txid_(0) {
  int ignore;

  // inode manager
  im = new inode_manager();

  // persistence

  _persister = new chfs_persister("log");  // DO NOT change the dir name here

  _persister->restore_checkpoint();
  _persister->restore_logdata();

  for (const auto &i : _persister->bin_entries) {
    switch (i.type_) {
      case chfs_command::CMD_CREATE:
        occupy(i.inum_, (i.data_[0] << 0) + (i.data_[1] << 8) +
                            (i.data_[2] << 16) + (i.data_[3] << 24));
        break;
      case chfs_command::CMD_PUT:
        put(i.inum_, 0, i.data_, ignore);
        break;
      case chfs_command::CMD_REMOVE:
        remove(i.inum_, 0, ignore);
        break;
      default:
        break;
    }
  }

  std::set<chfs_command::txid_t> finished;
  for (const auto &i : _persister->log_entries) {
    if (i.type_ == chfs_command::CMD_COMMIT) {
      finished.insert(i.txid_);
    }
  }

  for (const auto &i : _persister->log_entries) {
    if (finished.count(i.txid_) != 0) {
      switch (i.type_) {
        case chfs_command::CMD_CREATE:
          occupy(i.inum_, (i.data_[0] << 0) + (i.data_[1] << 8) +
                              (i.data_[2] << 16) + (i.data_[3] << 24));
          break;
        case chfs_command::CMD_PUT:
          put(i.inum_, 0, i.data_, ignore);
          break;
        case chfs_command::CMD_REMOVE:
          remove(i.inum_, 0, ignore);
          break;
        default:
          break;
      }
    } else {
      std::cout << __PRETTY_FUNCTION__ << ": uncommitted log " << i.txid_ << " "
                << i.type_ << std::endl;
    }
  }
  _persister->start_persist();
}

extent_protocol::status extent_server::create(uint32_t type,
                                              chfs_command::txid_t txid,
                                              extent_protocol::extentid_t &id) {
  id = im->alloc_inode(type);
  _persister->append_log({txid,
                          chfs_command::cmd_type::CMD_CREATE,
                          static_cast<uint32_t>(id),
                          {
                              static_cast<char>((type >> 0) & 0xff),
                              static_cast<char>((type >> 8) & 0xff),
                              static_cast<char>((type >> 16) & 0xff),
                              static_cast<char>((type >> 24) & 0xff),
                          }});

  return extent_protocol::OK;
}

extent_protocol::status extent_server::occupy(extent_protocol::extentid_t id,
                                              uint32_t type) {
  im->occupy_inode(id, type);

  return extent_protocol::OK;
}

int extent_server::put(extent_protocol::extentid_t id,
                       chfs_command::txid_t txid, std::string buf, int &) {
  id &= 0x7fffffff;

  _persister->append_log(
      {txid, chfs_command::cmd_type::CMD_PUT, static_cast<uint32_t>(id), buf});

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

int extent_server::remove(extent_protocol::extentid_t id,
                          chfs_command::txid_t txid, int &) {
  id &= 0x7fffffff;

  _persister->append_log({txid,
                          chfs_command::cmd_type::CMD_REMOVE,
                          static_cast<uint32_t>(id),
                          {}});

  im->remove_file(id);

  return extent_protocol::OK;
}

extent_protocol::status extent_server::start_tx(int ignore,
                                                chfs_command::txid_t &txid) {
  txid = ++txid_;
  _persister->append_log({txid, chfs_command::cmd_type::CMD_BEGIN, 0, {}});
  return extent_protocol::OK;
}

extent_protocol::status extent_server::commit_tx(chfs_command::txid_t txid,
                                                 int &ignore) {
  _persister->append_log({txid, chfs_command::cmd_type::CMD_COMMIT, 0, {}});
  _persister->checkpoint();
  return extent_protocol::OK;
}
extent_protocol::status extent_server::abort_tx(chfs_command::txid_t txid,
                                                int &ignore) {
  _persister->append_log({txid, chfs_command::cmd_type::CMD_ABORT, 0, {}});
  return extent_protocol::OK;
}
