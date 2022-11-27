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

extent_protocol::status
extent_client::create(uint32_t type,  extent_protocol::extentid_t &id) {
    extent_protocol::status ret = extent_protocol::OK;
    ret = cl->call(extent_protocol::create, type,  id);
    return ret;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf) {
    extent_protocol::status ret = extent_protocol::OK;
    ret = cl->call(extent_protocol::get, eid, buf);
    return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid,
                       extent_protocol::attr &attr) {
    extent_protocol::status ret = extent_protocol::OK;
    ret = cl->call(extent_protocol::getattr, eid, attr);
    return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf) {
    int r;
    extent_protocol::status ret = extent_protocol::OK;
    ret = cl->call(extent_protocol::put, eid, buf,  r);
    return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid) {
    int r = 0;
    extent_protocol::status ret = extent_protocol::OK;
    ret = cl->call(extent_protocol::remove, eid, r);
    return ret;
}


