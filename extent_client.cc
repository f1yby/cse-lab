// RPC stubs for clients to talk to extent_server

#include "extent_client.h"

#include <cstdio>
#include <iostream>

extent_client::extent_client(std::string dst) {
  sockaddr_in dstsock{};
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    std::cout << __PRETTY_FUNCTION__ << ": bind failed" << std::endl;
  }
}

extent_protocol::status extent_client::create(uint32_t type,
                                              chfs_command::txid_t txid,
                                              extent_protocol::extentid_t &id) {
  return cl->call(extent_protocol::create, type, txid, id);
}

extent_protocol::status extent_client::get(extent_protocol::extentid_t eid,
                                           std::string &buf) {
  return cl->call(extent_protocol::get, eid, buf);
}

extent_protocol::status extent_client::getattr(extent_protocol::extentid_t eid,
                                               extent_protocol::attr &attr) {
  return cl->call(extent_protocol::getattr, eid, attr);
}

extent_protocol::status extent_client::put(extent_protocol::extentid_t eid,
                                           std::string buf,
                                           chfs_command::txid_t txid) {
  int ignore;
  return cl->call(extent_protocol::put, eid, txid, buf, ignore);
}

extent_protocol::status extent_client::remove(extent_protocol::extentid_t eid,
                                              chfs_command::txid_t txid) {
  int ignore;
  return cl->call(extent_protocol::remove, eid, txid, ignore);
}

extent_protocol::status extent_client::start_tx(chfs_command::txid_t &txid) {
  int ignore{};
  return cl->call(extent_protocol::start_tx, ignore, txid);
}

extent_protocol::status extent_client::commit_tx(chfs_command::txid_t txid) {
  int ignore;
  return cl->call(extent_protocol::commit_tx, txid, ignore);
}

extent_protocol::status extent_client::abort_tx(chfs_command::txid_t txid) {
  int ignore;
  return cl->call(extent_protocol::abort_tx, txid, ignore);
}