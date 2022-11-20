// RPC stubs for clients to talk to extent_server

#include "extent_client.h"

#include <cstdio>
#include <iostream>

extent_client::extent_client() { es = new extent_server(); }

extent_protocol::status extent_client::create(uint32_t type,
                                              extent_protocol::extentid_t &id) {
  return es->create(type, id);
}

extent_protocol::status extent_client::get(extent_protocol::extentid_t eid,
                                           std::string &buf) {
  return es->get(eid, buf);
}

extent_protocol::status extent_client::getattr(extent_protocol::extentid_t eid,
                                               extent_protocol::attr &attr) {
  return es->getattr(eid, attr);
}

extent_protocol::status extent_client::put(extent_protocol::extentid_t eid,
                                           std::string buf) {
  int ignore;
  return es->put(eid, buf, ignore);
}

extent_protocol::status extent_client::remove(extent_protocol::extentid_t eid) {
  int ignore;
  return es->remove(eid, ignore);
}
