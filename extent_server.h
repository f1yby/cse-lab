// this is the extent server
#pragma once

#include <map>
#include <string>

#include "extent_protocol.h"
#include "inode_manager.h"

class extent_server {
 public:
  inode_manager *im;

 public:
  extent_server();
  extent_protocol::status create(uint32_t type, extent_protocol::extentid_t &);
  extent_protocol::status occupy(extent_protocol::extentid_t, uint32_t type);
  extent_protocol::status put(extent_protocol::extentid_t, std::string,
                              int &ignore);
  extent_protocol::status get(extent_protocol::extentid_t, std::string &);
  extent_protocol::status getattr(extent_protocol::extentid_t,
                                  extent_protocol::attr &);
  extent_protocol::status remove(extent_protocol::extentid_t id, int &ignore);
};