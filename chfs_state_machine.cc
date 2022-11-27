#include "chfs_state_machine.h"

chfs_command_raft::chfs_command_raft() : cmd_tp(CMD_NONE), type(0), id(0) {}

chfs_command_raft::chfs_command_raft(const chfs_command_raft &cmd)
    : cmd_tp(cmd.cmd_tp),
      type(cmd.type),
      id(cmd.id),
      buf(cmd.buf),
      res(cmd.res) {
  // Lab3: Your code here
}
chfs_command_raft::~chfs_command_raft() {
  // Lab3: Your code here
}

int chfs_command_raft::size() const {
  int size = 0;
  size += sizeof(cmd_tp);
  size += sizeof(type);
  size += sizeof(id);
  size += buf.size();
  return size;
}

void chfs_command_raft::serialize(char *buf_out, int size) const {
  // Lab3: Your code here
  if (size != this->size()) {
    abort();
  }
  int offset = 0;
  memcpy(buf_out + offset, static_cast<const void *>(&cmd_tp), sizeof(cmd_tp));
  offset += sizeof(cmd_tp);

  memcpy(buf_out + offset, static_cast<const void *>(&type), sizeof(type));
  offset += sizeof(type);

  memcpy(buf_out + offset, static_cast<const void *>(&id), sizeof(id));
  offset += sizeof(id);

  memcpy(buf_out + offset, &buf[0], buf.size());
}

void chfs_command_raft::deserialize(const char *buf_in, int size) {
  // Lab3: Your code here
  int offset = 0;
  if (size < sizeof(cmd_tp) + sizeof(type) + sizeof(id)) {
    abort();
  }

  memcpy(static_cast<void *>(&cmd_tp), buf_in + offset, sizeof(cmd_tp));
  offset += sizeof(cmd_tp);

  memcpy(static_cast<void *>(&type), buf_in + offset, sizeof(type));
  offset += sizeof(type);

  memcpy(static_cast<void *>(&id), buf_in + offset, sizeof(id));
  offset += sizeof(id);

  buf.resize(size - offset);
  memcpy(&buf[0], buf_in + offset, buf.size());
}

marshall &operator<<(marshall &m, const chfs_command_raft &cmd) {
  m << cmd.cmd_tp;
  m << cmd.type;
  m << cmd.id;
  m << cmd.buf;
  return m;
}

unmarshall &operator>>(unmarshall &u, chfs_command_raft &cmd) {
  // Lab3: Your code here
  int tp;
  u >> tp;
  cmd.cmd_tp = static_cast<chfs_command_raft::command_type>(tp);
  u >> cmd.type;
  u >> cmd.id;
  u >> cmd.buf;

  return u;
}

void chfs_state_machine::apply_log(raft_command &cmd) {
  int ignore;
  uint32_t uignore;
  extent_protocol::extentid_t id_ignore;
  std::unique_lock<std::mutex> l(mtx);
  auto &chfs_cmd = static_cast<chfs_command_raft &>(cmd);
  if (chfs_cmd.res) {
    std::unique_lock<std::mutex> lc(chfs_cmd.res->mtx);
    switch (chfs_cmd.cmd_tp) {
      case chfs_command_raft::CMD_NONE:
        break;
      case chfs_command_raft::CMD_CRT:
        es.create(chfs_cmd.type, chfs_cmd.res->id);
        break;
      case chfs_command_raft::CMD_PUT:
        es.put(chfs_cmd.id, chfs_cmd.buf, ignore);
        break;
      case chfs_command_raft::CMD_GET:
        es.get(chfs_cmd.id, chfs_cmd.res->buf);
        break;
      case chfs_command_raft::CMD_GETA:
        es.getattr(chfs_cmd.id, chfs_cmd.res->attr);
        break;
      case chfs_command_raft::CMD_RMV:
        es.remove(chfs_cmd.id, ignore);
        break;
    }
    chfs_cmd.res->done = true;
    chfs_cmd.res->cv.notify_all();
  } else {
    switch (chfs_cmd.cmd_tp) {
      case chfs_command_raft::CMD_NONE:
        break;
      case chfs_command_raft::CMD_CRT:
        es.create(chfs_cmd.type, id_ignore);
        break;
      case chfs_command_raft::CMD_PUT:
        es.put(chfs_cmd.id, chfs_cmd.buf, ignore);
        break;
      case chfs_command_raft::CMD_GET:
      case chfs_command_raft::CMD_GETA:
        break;
      case chfs_command_raft::CMD_RMV:
        es.remove(chfs_cmd.id, ignore);
        break;
    }
  }
}
