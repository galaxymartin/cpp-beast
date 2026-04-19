#include "book_repository.hpp"
#include <fstream>
#include <filesystem>
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
  if (!persist_file_.empty()) {
    auto arr = build_snapshot_unlocked();
    lock.unlock();
    try {
      write_snapshot(arr);
    } catch (std::exception const& e) {
      BOOST_LOG_TRIVIAL(error) << "Failed to write snapshot after update: " << e.what();
    }
  }
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
  } catch (std::exception const& e) {
    BOOST_LOG_TRIVIAL(error) << "Failed to load persist file " << persist_file_ << ": " << e.what();
  } catch (...) {
    BOOST_LOG_TRIVIAL(error) << "Failed to load persist file " << persist_file_ << ": unknown error";
  }
}

boost::json::array BookRepository::build_snapshot_unlocked() const {
  boost::json::array arr;
  for (auto const& [k, v] : store_) { boost::json::value jv; to_json(jv, v); arr.push_back(jv); }
  return arr;
}

void BookRepository::write_snapshot(boost::json::array const& arr) const {
  try {
    std::filesystem::path p(persist_file_);
    std::string tmp = p.string() + ".tmp";
    {
      std::ofstream ofs(tmp, std::ios::trunc);
      if (!ofs) throw std::runtime_error("failed to open temp file for write");
      ofs << boost::json::serialize(arr);
      if (!ofs) throw std::runtime_error("failed while writing temp file");
    }
    std::error_code ec;
    std::filesystem::rename(tmp, p, ec);
    if (ec) {
      // try remove tmp and report
      std::filesystem::remove(tmp);
      throw std::runtime_error(std::string("rename failed: ") + ec.message());
    }
  } catch (std::exception const& e) {
    BOOST_LOG_TRIVIAL(error) << "BookRepository::write_snapshot error: " << e.what();
    throw;
  }
}

// Note: saving is done by callers without holding the mutex to avoid deadlocks.
