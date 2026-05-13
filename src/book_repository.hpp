#pragma once

#include <optional>
#include <vector>
#include <unordered_map>
#include <shared_mutex>

#include "book.hpp"

class BookRepository {
public:
  // If `persist_file` is non-empty, repository will load/save to that file.
  explicit BookRepository(std::string persist_file = {});

  Book add(Book b);
  std::optional<Book> get(uint64_t id) const;
  std::optional<Book> update(uint64_t id, Book const& b);
  std::vector<Book> all() const;
  bool remove(uint64_t id);

private:
  mutable std::shared_mutex mutex_;
  std::unordered_map<uint64_t, Book> store_;
  uint64_t next_id_ = 1;
  std::string persist_file_;
  void load_from_file();
  // Build JSON snapshot from `store_`. MUST be called while holding the lock.
  boost::json::array build_snapshot_unlocked() const;
  // Write a snapshot to `persist_file_` (does not take locks).
  void write_snapshot(boost::json::array const& arr) const;
};
