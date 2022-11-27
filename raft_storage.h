#ifndef raft_storage_h
#define raft_storage_h

#include <fcntl.h>
#include <sys/stat.h>

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
  std::string snapshot_path;

  std::fstream commit;
  std::fstream metadata;
  std::fstream snapshot;

  // Weak Commit
  std::vector<command> weak;

  // Snapshot
  std::vector<char> data;

  // Metadata
  bool valid;
  int role;
  int current_term;
  int leader_id;
  int vote_for;
  int weak_commit;
  int strong_commit;
  int weak_commit_term;
  int strong_commit_size;

  // Operations on log
  void flush_log(const std::vector<command>& entries, int committed_in_memory) {
    if (entries.empty()) {
      return;
    }
    truncate(commit_path.c_str(), strong_commit_size);
    commit.open(commit_path, std::ios::app);
    for (int i = committed_in_memory; i < entries.size(); ++i) {
      auto& e = entries[i];
      int size = e.size();
      auto buffer = std::string(size, '\0');
      e.serialize(&buffer[0], size);
      commit.write(reinterpret_cast<const char*>(&size), sizeof(size));
      commit.write(buffer.c_str(), buffer.size());
    }
    commit.flush();
    commit.close();
  }

  void clear_log() {
    truncate(commit_path.c_str(), 0);
    strong_commit_size = 0;
    weak_commit = strong_commit;
    flush_metadata();
  }

  void update_strong_commit_size(const std::vector<command>& entries,
                                 int committed_in_memory, int size) {
    for (int i = 0; i < size; ++i) {
      auto ii = i + committed_in_memory;
      strong_commit_size += entries[ii].size() + sizeof(entries[ii].size());
    }
    flush_metadata();
  }

  void load_log() {
    int size;
    std::string buffer;
    commit.open(commit_path, std::ios::in);
    commit.seekg(strong_commit_size);
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

  // Operations on snapshot
  void load_snapshot() {
    snapshot.open(snapshot_path, std::ios::in);
    if (!snapshot.good()) {
      return;
    }
    struct stat statbuf {};
    stat(snapshot_path.c_str(), &statbuf);
    auto size = statbuf.st_size;
    data.resize(size);
    snapshot.read(&data[0], data.size());
    snapshot.close();
  }

  void flush_snapshot() {
    snapshot.open(snapshot_path, std::ios::out);
    snapshot.write(data.data(), data.size());
    snapshot.flush();
    snapshot.close();
  }

  // Operations on metadata
  void load_metadata() {
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
    metadata >> weak_commit;
    metadata >> strong_commit;
    metadata >> weak_commit_term;
    metadata >> strong_commit_size;
    valid = true;
  }

  void flush_metadata() {
    metadata.close();
    metadata.open(metadata_file_path, std::ios::out | std::ios::trunc);
    metadata << role << std::endl;
    metadata << current_term << std::endl;
    metadata << leader_id << std::endl;
    metadata << vote_for << std::endl;
    metadata << weak_commit << std::endl;
    metadata << strong_commit << std::endl;
    metadata << weak_commit_term << std::endl;
    metadata << strong_commit_size << std::endl;
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
  snapshot_path = dir + "/snapshot.bin";
  metadata_file_path = dir + "/meta.txt";

  // Load metadata
  load_metadata();
  if (!valid) {
    return;
  }

  // Load weak commit
  load_log();
  if (weak.size() < weak_commit) {
    weak_commit = strong_commit;
  }

  // Load snapshot
  load_snapshot();
}

template <typename command>
raft_storage<command>::~raft_storage() {
  // Lab3: Your code here
  flush_metadata();
}

#endif  // raft_storage_h