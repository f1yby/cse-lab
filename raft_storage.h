#ifndef raft_storage_h
#define raft_storage_h

#include <fcntl.h>

#include <filesystem>
#include <fstream>
#include <mutex>

#include "raft_protocol.h"

template <typename command>
class raft_storage {
 public:
  raft_storage(const std::string& file_dir);
  ~raft_storage();
  // Lab3: Your code here

  // File stream
  std::string commit_path;
  std::string metadata_file_path;

  std::fstream commit;
  std::fstream metadata;

  // Weak Commit
  std::vector<command> weak;

  // Metadata
  bool valid;
  int role;
  int current_term;
  int leader_id;
  int vote_for;
  int weak_commit_size;
  int strong_commit_size;
  int weak_commit_term;
  int last_commit_size;

  // Operations on log
  void convert_to_commit(command c) {
    last_commit_size += c.size() + sizeof(c.size());
    flush_metadata();
  }

  void append_weak(command c) {
    int size = c.size();
    auto buffer = std::string(size, '\0');
    c.serialize(&buffer[0], size);

    commit.open(commit_path, std::ios::app);
    commit.write(reinterpret_cast<const char*>(&size), sizeof(size));
    commit.write(buffer.c_str(), buffer.size());
    commit.flush();
    commit.close();
  }

  void sync_log(std::vector<command> entries) {
    commit.open(commit_path, std::ios::out);
    last_commit_size = 0;
    for (int i = 0; i < strong_commit_size; ++i) {
      last_commit_size += entries[i].size() + sizeof(entries[i].size());
    }

    for (const auto& e : entries) {
      int size = e.size();
      auto buffer = std::string(size, '\0');
      e.serialize(&buffer[0], size);
      commit.write(reinterpret_cast<const char*>(&size), sizeof(size));
      commit.write(buffer.c_str(), buffer.size());
    }
    commit.flush();
    commit.close();
    flush_metadata();
  }

  void reload_weak() {
    int size;
    std::string buffer;
    // FIXME only load weak
    commit.open(commit_path, std::ios::in);
    commit.seekg(0);
    //    commit.seekg(last_commit_size);
    while (commit.good() && !commit.eof()) {
      commit.read(reinterpret_cast<char*>(&size), sizeof(size));
      buffer.resize(size);
      commit.read(&buffer[0], size);
      command c;
      c.deserialize(buffer.c_str(), size);
      weak.push_back(c);
    }
    commit.flush();
    commit.close();
  }

  void clear_weak_commit() { truncate(commit_path.c_str(), last_commit_size); }

  void flush_metadata() {
    metadata.close();
    metadata.open(metadata_file_path, std::ios::out | std::ios::trunc);
    metadata << role << std::endl;
    metadata << current_term << std::endl;
    metadata << leader_id << std::endl;
    metadata << vote_for << std::endl;
    metadata << weak_commit_size << std::endl;
    metadata << strong_commit_size << std::endl;
    metadata << weak_commit_term << std::endl;
    metadata << last_commit_size << std::endl;
    metadata.flush();
  }

 private:
  std::mutex mtx;
  // Lab3: Your code here
};

template <typename command>
raft_storage<command>::raft_storage(const std::string& dir) {
  // Lab3: Your code here
  commit_path = dir + "/commit.bin";
  metadata_file_path = dir + "/meta.txt";

  metadata.open(metadata_file_path, std::ios::in);
  // Read metadata
  if (!metadata.good()) {
    valid = false;
    return;
  }
  metadata >> role;
  metadata >> current_term;
  metadata >> leader_id;
  metadata >> vote_for;
  metadata >> weak_commit_size;
  metadata >> strong_commit_size;
  metadata >> weak_commit_term;
  metadata >> last_commit_size;
  valid = true;

  // Read weak commit
  reload_weak();
  if (weak.size() < weak_commit_size) {
    weak_commit_size = strong_commit_size;
  }
  assert(weak.size() >= strong_commit_size);
}

template <typename command>
raft_storage<command>::~raft_storage() {
  // Lab3: Your code here
  flush_metadata();
}

#endif  // raft_storage_h