// the extent server implementation

#include "extent_server.h"
#include <cstdlib>
#include <sstream>

extent_server::extent_server() : txid_(0) {
  im = new inode_manager();
  _persister = new chfs_persister("log");// DO NOT change the dir name here
  _persister->restore_logdata();

  std::set<chfs_command::txid_t> finished;
  for (const auto &i: _persister->log_entries) {
    if (i.type_ == chfs_command::CMD_COMMIT) { finished.insert(i.txid_); }
    std::cout << i.txid_ << " finished" << std::endl;
  }

  extent_protocol::extentid_t id = 0;
  for (const auto &i: _persister->log_entries) {
    if (finished.count(i.txid_) != 0) {
      switch (i.type_) {
        case chfs_command::CMD_CREATE:
          create(i.inum_, id, 0);
          break;
        case chfs_command::CMD_PUT:
          put(i.inum_, i.data_, 0);
          break;
        case chfs_command::CMD_REMOVE:
          remove(i.inum_, 0);
          break;
        default:
          break;
      }
    }
  }
  _persister->start_persist();
}


int extent_server::create(uint32_t type, extent_protocol::extentid_t &id,
                          chfs_command::txid_t txid) {

  _persister->append_log({txid, chfs_command::cmd_type::CMD_CREATE, type, {}});

  id = im->alloc_inode(type);

  return extent_protocol::OK;
}


int extent_server::put(extent_protocol::extentid_t id, std::vector<uint8_t> buf,
                       chfs_command::txid_t txid) {
  id &= 0x7fffffff;

  _persister->append_log(
      {txid, chfs_command::cmd_type::CMD_PUT, static_cast<uint32_t>(id), buf});

  const auto *cbuf = static_cast<const uint8_t *>(buf.data());
  auto size = buf.size();
  im->write_file(id, cbuf, size);

  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id,
                       std::vector<uint8_t> &buf) {
  id &= 0x7fffffff;

  uint32_t size = 0;
  uint8_t *cbuf = nullptr;

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
                          chfs_command::txid_t txid) {
  id &= 0x7fffffff;


  _persister->append_log({txid,
                          chfs_command::cmd_type::CMD_REMOVE,
                          static_cast<uint32_t>(id),
                          {}});

  im->remove_file(id);


  return extent_protocol::OK;
}

extent_protocol::status extent_server::start_tx(chfs_command::txid_t &txid) {
  txid = ++txid_;
  _persister->append_log({txid, chfs_command::cmd_type::CMD_BEGIN, 0, {}});
  return extent_protocol::OK;
}

extent_protocol::status extent_server::commit_tx(chfs_command::txid_t txid) {
  _persister->append_log({txid, chfs_command::cmd_type::CMD_COMMIT, 0, {}});
  return extent_protocol::OK;
}
extent_protocol::status extent_server::abort_tx(chfs_command::txid_t txid) {
  _persister->append_log({txid, chfs_command::cmd_type::CMD_ABORT, 0, {}});
  return extent_protocol::OK;
}
