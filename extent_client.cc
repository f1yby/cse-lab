// RPC stubs for clients to talk to extent_server

#include "extent_client.h"

#include <iostream>
#include <utility>

extent_client::extent_client() { es = new extent_server(); }

extent_protocol::status extent_client::create(uint32_t type,
                                              extent_protocol::extentid_t &id,
                                              chfs_command::txid_t txid) {
  return es->create(id, type, txid);
}

extent_protocol::status extent_client::get(extent_protocol::extentid_t eid,
                                           std::vector<uint8_t> &buf) {
  return es->get(eid, buf);
}

extent_protocol::status extent_client::getattr(extent_protocol::extentid_t eid,
                                               extent_protocol::attr &attr) {
  return es->getattr(eid, attr);
}

extent_protocol::status extent_client::put(extent_protocol::extentid_t eid,
                                           std::vector<uint8_t> buf,
                                           chfs_command::txid_t txid) {
  return es->put(eid, std::move(buf), txid);
}

extent_protocol::status extent_client::remove(extent_protocol::extentid_t eid,
                                              chfs_command::txid_t txid) {
  return es->remove(eid, txid);
}
extent_protocol::status extent_client::start_tx(chfs_command::txid_t &txid) {
  return es->start_tx(txid);
}
extent_protocol::status extent_client::commit_tx(chfs_command::txid_t txid) {
  return es->commit_tx(txid);
}
extent_protocol::status extent_client::abort_tx(chfs_command::txid_t txid) {
  return es->abort_tx(txid);
}
