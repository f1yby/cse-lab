#include "extent_server_dist.h"

#include <utility>

chfs_raft *extent_server_dist::leader() const {
  int leader = this->raft_group->check_exact_one_leader();
  if (leader < 0) {
    return this->raft_group->nodes[0];
  } else {
    return this->raft_group->nodes[leader];
  }
}

int extent_server_dist::create(uint32_t type, extent_protocol::extentid_t &id) {
  int ignore;
  auto res = std::make_shared<chfs_command_raft::result>();
  std::unique_lock<std::mutex> l(res->mtx);
  leader()->new_command(
      chfs_command_raft{chfs_command_raft::CMD_CRT, type, 0, {}, res}, ignore,
      ignore);
  res->cv.wait(l);
  id = res->id;
  return extent_protocol::OK;
}

int extent_server_dist::put(extent_protocol::extentid_t id, std::string buf,
                            int &) {
  int ignore;
  auto res = std::make_shared<chfs_command_raft::result>();
  std::unique_lock<std::mutex> l(res->mtx);
  leader()->new_command(
      chfs_command_raft{chfs_command_raft::CMD_PUT, 0, 0, std::move(buf), res},
      ignore, ignore);
  while (!res->done) {
    res->cv.wait(l);
  }

  return extent_protocol::OK;
}

int extent_server_dist::get(extent_protocol::extentid_t id, std::string &buf) {
  int ignore;
  auto res = std::make_shared<chfs_command_raft::result>();
  std::unique_lock<std::mutex> l(res->mtx);
  leader()->new_command(
      chfs_command_raft{chfs_command_raft::CMD_GET, 0, 0, {}, res}, ignore,
      ignore);
  res->cv.wait(l);
  buf = res->buf;
  return extent_protocol::OK;
}

int extent_server_dist::getattr(extent_protocol::extentid_t id,
                                extent_protocol::attr &a) {
  int ignore;
  auto res = std::make_shared<chfs_command_raft::result>();
  std::unique_lock<std::mutex> l(res->mtx);
  leader()->new_command(
      chfs_command_raft{chfs_command_raft::CMD_GETA, 0, id, {}, res}, ignore,
      ignore);
  res->cv.wait(l);
  a = res->attr;
  return extent_protocol::OK;
}

int extent_server_dist::remove(extent_protocol::extentid_t id, int &) {
  int ignore;
  auto res = std::make_shared<chfs_command_raft::result>();
  std::unique_lock<std::mutex> l(res->mtx);
  leader()->new_command(
      chfs_command_raft{chfs_command_raft::CMD_RMV, 0, id, {}, res}, ignore,
      ignore);
  res->cv.wait(l);
  return extent_protocol::OK;
}

extent_server_dist::~extent_server_dist() { delete this->raft_group; }