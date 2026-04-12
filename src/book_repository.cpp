#include "book_repository.hpp"
#include <fstream>
#include <boost/json/src.hpp>
#include <boost/json.hpp>
#include <boost/log/trivial.hpp>

Book BookRepository::add(Book b) {
  std::unique_lock lock(mutex_);
  b.id = next_id_++;
  store_.emplace(b.id, b);
  BOOST_LOG_TRIVIAL(info) << "BookRepository::add id=" << b.id << " title='" << b.title << "'";
  if (!persist_file_.empty()) {
    // create snapshot while holding lock, then write file after unlocking to avoid deadlock
    auto arr = build_snapshot_unlocked();
    lock.unlock();
    write_snapshot(arr);
  }
  return b;
}

std::optional<Book> BookRepository::get(uint64_t id) const {
  std::shared_lock lock(mutex_);
  auto it = store_.find(id);
  if (it == store_.end()) return std::nullopt;
  BOOST_LOG_TRIVIAL(info) << "BookRepository::get id=" << id << " hit";
  return it->second;
}

std::optional<Book> BookRepository::update(uint64_t id, Book const& b) {
  std::unique_lock lock(mutex_);
  auto it = store_.find(id);
  if (it == store_.end()) return std::nullopt;
  // PUT semantics: replace the stored resource with provided fields (keep id)
  Book nb = b;
  nb.id = id;
  it->second = nb;
  BOOST_LOG_TRIVIAL(info) << "BookRepository::update id=" << id << " title='" << nb.title << "'";
  return nb;
}

std::vector<Book> BookRepository::all() const {
  std::shared_lock lock(mutex_);
  std::vector<Book> out;
  out.reserve(store_.size());
  for (auto const& [k, v] : store_) out.push_back(v);
  BOOST_LOG_TRIVIAL(info) << "BookRepository::all count=" << out.size();
  return out;
}

bool BookRepository::remove(uint64_t id) {
  std::unique_lock lock(mutex_);
  auto removed = store_.erase(id) > 0;
  BOOST_LOG_TRIVIAL(info) << "BookRepository::remove id=" << id << " removed=" << removed;
  if (removed && !persist_file_.empty()) {
    auto arr = build_snapshot_unlocked();
    lock.unlock();
    write_snapshot(arr);
  }
  return removed;
}

BookRepository::BookRepository(std::string persist_file) : persist_file_(std::move(persist_file)) {
  if (!persist_file_.empty()) load_from_file();
}

void BookRepository::load_from_file() {
  std::unique_lock lock(mutex_);
  try {
    std::ifstream ifs(persist_file_);
    if (!ifs) return;
    std::string contents((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (contents.empty()) return;
    auto v = boost::json::parse(contents);
    auto arr = v.as_array();
    store_.clear();
    next_id_ = 1;
    for (auto const& item : arr) {
      Book b;
      from_json(item, b);
      store_.emplace(b.id, b);
      if (b.id >= next_id_) next_id_ = b.id + 1;
    }
  } catch (...) {
    // ignore load errors
  }
}

boost::json::array BookRepository::build_snapshot_unlocked() const {
  boost::json::array arr;
  for (auto const& [k, v] : store_) { boost::json::value jv; to_json(jv, v); arr.push_back(jv); }
  return arr;
}

void BookRepository::write_snapshot(boost::json::array const& arr) const {
  try {
    std::ofstream ofs(persist_file_, std::ios::trunc);
    if (ofs) ofs << boost::json::serialize(arr);
  } catch (...) {}
}

// Note: saving is done by callers without holding the mutex to avoid deadlocks.
