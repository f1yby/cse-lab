// this is the extent server
#pragma once

#include <map>
#include <string>

#include "extent_protocol.h"
#include "inode_manager.h"
#include "persister.h"

class extent_server {
 protected:
  inode_manager *im;
  chfs_persister *_persister;
  chfs_command::txid_t txid_;

 public:
  extent_server();
  extent_protocol::status create(uint32_t type, chfs_command::txid_t txid,
                                 extent_protocol::extentid_t &);
  extent_protocol::status occupy(extent_protocol::extentid_t, uint32_t type);
  extent_protocol::status put(extent_protocol::extentid_t, chfs_command::txid_t,
                              std::string, int &ignore);
  extent_protocol::status get(extent_protocol::extentid_t, std::string &);
  extent_protocol::status getattr(extent_protocol::extentid_t,
                                  extent_protocol::attr &);
  extent_protocol::status remove(extent_protocol::extentid_t id,
                                 chfs_command::txid_t txid, int &ignore);
  extent_protocol::status start_tx(int ignore, chfs_command::txid_t &txid);
  extent_protocol::status commit_tx(chfs_command::txid_t txid, int &ignore);
  extent_protocol::status abort_tx(chfs_command::txid_t txid, int &ignore);
};