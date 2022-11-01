// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <map>
#include <string>

#include "extent_protocol.h"
#include "inode_manager.h"
#include "persister.h"

class extent_server {
 protected:
#if 0
  typedef struct extent {
    std::string data;
    struct extent_protocol::attr attr;
  } extent_t;
  std::map <extent_protocol::extentid_t, extent_t> extents;
#endif
  inode_manager *im;
  chfs_persister *_persister;
  chfs_command::txid_t txid_;

 public:
  extent_server();
  extent_protocol::status create(extent_protocol::extentid_t &id, uint32_t type,
                                 chfs_command::txid_t txid);
  extent_protocol::status occupy(extent_protocol::extentid_t id, uint32_t type);
  extent_protocol::status put(extent_protocol::extentid_t id,
                              std::vector<uint8_t>, chfs_command::txid_t txid);
  extent_protocol::status get(extent_protocol::extentid_t id,
                              std::vector<uint8_t> &);
  extent_protocol::status getattr(extent_protocol::extentid_t id,
                                  extent_protocol::attr &);
  extent_protocol::status remove(extent_protocol::extentid_t id,
                                 chfs_command::txid_t txid);
  extent_protocol::status start_tx(chfs_command::txid_t &txid);
  extent_protocol::status commit_tx(chfs_command::txid_t txid);
  extent_protocol::status abort_tx(chfs_command::txid_t txid);
};

#endif
