#pragma once

#include "book.hpp"
#include <optional>
#include <vector>
#include <unordered_map>
#include <shared_mutex>

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
};
