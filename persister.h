#ifndef persister_h
#define persister_h

#include "extent_server.h"
#include "rpc.h"
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <unistd.h>
#include <utility>

#define MAX_LOG_SZ 1024

class extent_server;

/*
 * Your code here for Lab2A:
 * Implement class chfs_command, you may need to add command types such as
 * 'create', 'put' here to represent different commands a transaction requires. 
 * 
 * Here are some tips:
 * 1. each transaction in ChFS consists of several chfs_commands.
 * 2. each transaction in ChFS MUST contain a BEGIN command and a COMMIT command.
 * 3. each chfs_commands contains transaction ID, command type, and other information.
 * 4. you can treat a chfs_command as a log entry.
 */
class chfs_command {
public:
  typedef unsigned long long txid_t;
  enum cmd_type {
    CMD_BEGIN = 0,
    CMD_COMMIT,
    CMD_CREATE,
    CMD_PUT,
    CMD_REMOVE,
  };

  txid_t txid_ = 0;
  uint32_t inum_ = 0;

  cmd_type type_ = CMD_BEGIN;

  // size | txid | inode | cmd_type | data
  std::vector<uint8_t> data_;

  std::vector<uint8_t> raw_data_;

  uint32_t cursor_ = 0;

  void encode(const void *data, uint32_t size) {
    memcpy(&(raw_data_[cursor_]), data, size);
    cursor_ += size;
  }

  void decode(void *data, uint32_t size) {
    memcpy(data, &raw_data_[cursor_], size);
    cursor_ += size;
  }

  // constructor
  chfs_command(txid_t txid, cmd_type type, uint32_t inum,
               const std::vector<uint8_t> &data)
      : txid_(txid), inum_(inum), type_(type), data_(data) {

    uint32_t size = sizeof(txid) + sizeof(inum) + sizeof(type_) + data.size();
    raw_data_.resize(sizeof(size) + size);

    encode(&size, sizeof(size));
    encode(&txid_, sizeof(txid_));
    encode(&inum_, sizeof(inum_));
    encode(&type_, sizeof(type_));
    encode(data.data(), data.size());
  }
  explicit chfs_command(std::vector<uint8_t> raw_data)
      : raw_data_(std::move(raw_data)) {
    uint32_t size = raw_data_.size();
    data_.resize(size - sizeof(txid_) - sizeof(inum_) - sizeof(type_));

    decode(&txid_, sizeof(txid_));
    decode(&inum_, sizeof(inum_));
    decode(&type_, sizeof(type_));
    decode(&data_[0], data_.size());
  }

  std::vector<uint8_t> into() {
    cursor_ = 0;
    return std::move(raw_data_);
  }

  [[nodiscard]] uint64_t size() const { return data_.size(); }
};

/*
 * Your code here for Lab2A:
 * Implement class persister. A persister directly interacts with log files.
 * Remember it should not contain any transaction logic, its only job is to 
 * persist and recover data.
 * 
 * P.S. When and how to do checkpoint is up to you. Just keep your logfile size
 *      under MAX_LOG_SZ and checkpoint file size under DISK_SIZE.
 */
template<typename command>
class persister {

public:
  explicit persister(const std::string &file_dir);
  ~persister();

  // persist data into solid binary file
  // You may modify parameters in these functions
  void append_log(command log);
  void checkpoint();

  // restore data from solid binary file
  // You may modify parameters in these functions
  void restore_logdata();
  void restore_checkpoint();

  [[nodiscard]] chfs_command::txid_t get_txid() const;
  void start_persist();

  std::vector<command> log_entries;

private:
  std::mutex mtx;
  std::string file_dir;
  std::string file_path_checkpoint;
  std::string file_path_logfile;
  chfs_command::txid_t txid_;
  bool start = false;
};

template<typename command>
persister<command>::persister(const std::string &dir) : txid_(0) {
  // DO NOT change the file names here
  file_dir = dir;
  file_path_checkpoint = file_dir + "/checkpoint.bin";
  file_path_logfile = file_dir + "/logdata.bin";

  int fpld = open(file_path_logfile.c_str(), O_CREAT | O_EXCL);
  if (fpld > 0) { close(fpld); }
  int fpcd = open(file_path_checkpoint.c_str(), O_CREAT | O_EXCL);
  if (fpcd > 0) { close(fpcd); }
}

template<typename command>
persister<command>::~persister() {
  // Your code here for lab2A
}

template<typename command>
void persister<command>::append_log(command log) {
  if (!start) { return; }
  log_entries.push_back(log);
  std::vector<uint8_t> raw = log.into();
  int out = open(file_path_logfile.c_str(), O_WRONLY | O_APPEND);
  if (out < 0) { return; }
  while (true) {
    ssize_t w = write(out, raw.data(), raw.size());
    if (w >= 0 || (w == -1 && errno != EINTR)) { break; }
  }
  close(out);
}

template<typename command>
void persister<command>::checkpoint() {
  // Your code here for lab2A
}

template<typename command>
void persister<command>::restore_logdata() {
  int in = open(file_path_logfile.c_str(), O_RDONLY);
  if (in < 0) { return; }
  while (true) {
    uint32_t size;
    ssize_t ar = read(in, &size, sizeof(size));
    if (ar == 0) { break; }
    if (ar == -1 && errno == EINTR) { continue; }
    auto raw = std::vector<uint8_t>();
    raw.resize(size);
    ar = read(in, &raw[0], size);
    if (ar == 0) {
      std::cout << "in is broken" << std::endl;
      break;
    }
    if (ar == -1 && errno == EINTR) { continue; }
    command c = command(raw);
    log_entries.push_back(c);
    txid_ = std::max(txid_, c.txid_);
  }
  close(in);
  std::cout << __PRETTY_FUNCTION__ << ": restored " << log_entries.size()
            << " log" << std::endl;
};

template<typename command>
void persister<command>::restore_checkpoint() {
  // Your code here for lab2A
}
template<typename command>
chfs_command::txid_t persister<command>::get_txid() const {
  return txid_;
}
template<typename command>
void persister<command>::start_persist() {
  start = true;
};

using chfs_persister = persister<chfs_command>;

#endif// persister_h