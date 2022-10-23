// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include "extent_protocol.h"
#include "extent_server.h"
#include <string>

class extent_client {
private:
  extent_server *es;

public:
  extent_client();

  extent_protocol::status create(uint32_t type,
                                 extent_protocol::extentid_t &eid,
                                 chfs_command::txid_t txid);
  extent_protocol::status get(extent_protocol::extentid_t eid,
                              std::vector<uint8_t> &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid,
                                  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid,
                              std::vector<uint8_t> buf,
                              chfs_command::txid_t txid);
  extent_protocol::status remove(extent_protocol::extentid_t eid,
                                 chfs_command::txid_t txid);
  extent_protocol::status start_tx(chfs_command::txid_t &txid);
  extent_protocol::status commit_tx(chfs_command::txid_t txid);
  extent_protocol::status abort_tx(chfs_command::txid_t txid);
};

#endif
