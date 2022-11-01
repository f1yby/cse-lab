// RPC stubs for clients to talk to lock_server

#include "lock_client.h"

#include <arpa/inet.h>
#include <stdio.h>

#include <iostream>
#include <sstream>

#include "rpc.h"

lock_client::lock_client(std::string dst) {
  sockaddr_in dstsock{};
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() < 0) {
    std::cout << __PRETTY_FUNCTION__ << ": call bind" << std::endl;
  }
}

int lock_client::stat(lock_protocol::lockid_t lid) {
  int stat;
  cl->call(lock_protocol::stat, cl->id(), lid, stat);
  return stat;
}

lock_protocol::status lock_client::acquire(lock_protocol::lockid_t lid) {
  int ignore;
  int ret = cl->call(lock_protocol::acquire, cl->id(), lid, ignore);
  if (ret < 0) {
    return lock_protocol::xxstatus::RPCERR;
  }
  if (ret > 0) {
    return lock_protocol::xxstatus::IOERR;
  }
  return lock_protocol::xxstatus::OK;
}

lock_protocol::status lock_client::release(lock_protocol::lockid_t lid) {
  int ignore;
  int ret = cl->call(lock_protocol::release, cl->id(), lid, ignore);
  if (ret < 0) {
    return lock_protocol::xxstatus::RPCERR;
  }
  if (ret > 0) {
    return lock_protocol::xxstatus::IOERR;
  }
  return lock_protocol::xxstatus::OK;
}