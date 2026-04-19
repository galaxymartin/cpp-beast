#include "http_server.hpp"
#include "book.hpp"
#include "http_util.hpp"
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <boost/log/trivial.hpp>
#include <set>
#include <mutex>
#include <memory>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

static void send_json_error(http::response<http::string_body>& res, http::status status, std::string_view message) {
  boost::json::object obj;
  obj["error"] = std::string(message);
  res.result(status);
  res.set(http::field::content_type, CONTENT_TYPE_JSON);
  res.body() = boost::json::serialize(obj);
  res.prepare_payload();
}

// Send a pre-serialized JSON value as a response (avoids repeating headers/body setup)
static void send_json_value(http::response<http::string_body>& res, http::status status, boost::json::value const& v) {
  res.result(status);
  res.set(http::field::content_type, CONTENT_TYPE_JSON);
  res.body() = boost::json::serialize(v);
  res.prepare_payload();
}

static bool validate_book_for_create(boost::json::value const& jv, std::string& err) {
  try {
    auto const& obj = jv.as_object();
    if (!obj.contains("title") || obj.at("title").as_string().empty()) { err = "title required"; return false; }
    if (!obj.contains("author") || obj.at("author").as_string().empty()) { err = "author required"; return false; }
    if (!obj.contains("year") || !obj.at("year").is_int64()) { err = "year required and must be integer"; return false; }
    return true;
  } catch (...) { err = "invalid json"; return false; }
}

static void handle_request(BookRepository& repo, http::request<http::string_body> req,
                           http::response<http::string_body>& res) {
  try {
    BOOST_LOG_TRIVIAL(info) << "HTTP " << req.method_string() << " " << req.target();

    // GET /books
    if (req.method() == http::verb::get && req.target() == "/books") {
      auto books = repo.all();
      boost::json::array arr;
      for (auto& b : books) { boost::json::value jv; to_json(jv, b); arr.push_back(jv); }
      send_json_value(res, http::status::ok, arr);
      return;
    }

    // GET /books/{id}
    if (req.method() == http::verb::get) {
      std::string_view target{req.target()};
      uint64_t id = 0;
      if (parse_book_id(target, id)) {
        auto ob = repo.get(id);
        if (!ob) { BOOST_LOG_TRIVIAL(info) << "Book not found id=" << id; send_json_error(res, http::status::not_found, ERR_BOOK_NOT_FOUND); return; }
        boost::json::value jv; to_json(jv, *ob);
        send_json_value(res, http::status::ok, jv);
        return;
      }
    }

    // POST /books
    if (req.method() == http::verb::post && req.target() == "/books") {
      boost::json::value jv;
      if (!parse_json_body(req.body(), jv)) { send_json_error(res, http::status::bad_request, ERR_INVALID_JSON); return; }
      std::string verr;
      if (!validate_book_for_create(jv, verr)) { send_json_error(res, http::status::bad_request, verr); return; }
      Book b; from_json(jv, b);
      auto added = repo.add(b);
      boost::json::value out; to_json(out, added);
      BOOST_LOG_TRIVIAL(info) << "Book created id=" << added.id;
      send_json_value(res, http::status::created, out);
      return;
    }

    // PUT /books/{id}
    if (req.method() == http::verb::put) {
      std::string_view target{req.target()};
      uint64_t id = 0;
      if (parse_book_id(target, id)) {
        boost::json::value jv;
        if (!parse_json_body(req.body(), jv)) { send_json_error(res, http::status::bad_request, ERR_INVALID_JSON); return; }
        Book b; from_json(jv, b);
        auto updated = repo.update(id, b);
        if (!updated) { BOOST_LOG_TRIVIAL(info) << "Book update not found id=" << id; send_json_error(res, http::status::not_found, ERR_BOOK_NOT_FOUND); return; }
        boost::json::value out; to_json(out, *updated);
        BOOST_LOG_TRIVIAL(info) << "Book updated id=" << id;
        send_json_value(res, http::status::ok, out);
        return;
      }
    }

    // DELETE /books/{id}
    if (req.method() == http::verb::delete_) {
      std::string_view target{req.target()};
      uint64_t id = 0;
      if (parse_book_id(target, id)) {
        auto removed = repo.remove(id);
        BOOST_LOG_TRIVIAL(info) << "Book delete id=" << id << " removed=" << removed;
        if (!removed) { send_json_error(res, http::status::not_found, ERR_BOOK_NOT_FOUND); return; }
        res.result(http::status::no_content);
        res.prepare_payload();
        return;
      }
    }

    // Not found
    send_json_error(res, http::status::not_found, ERR_NOT_FOUND);
    return;
  } catch (std::exception const& e) {
    BOOST_LOG_TRIVIAL(error) << "HTTP handler exception: " << e.what();
    send_json_error(res, http::status::internal_server_error, std::string("error: ") + e.what());
    return;
  }
}

// Internal server state shared between control handle and running server
struct ServerControl::Impl {
  net::io_context ioc{1};
  tcp::acceptor acceptor{ioc};
  std::mutex sessions_mutex;
  std::set<std::shared_ptr<tcp::socket>> sessions;
  std::vector<std::thread> workers;
  unsigned thread_count = 0;
  BookRepository* repo = nullptr;
  bool running = false;
  std::unique_ptr<net::signal_set> signals;
  Impl(unsigned short port): acceptor(ioc, tcp::endpoint{tcp::v4(), port}) {}
};

static void do_session(std::shared_ptr<tcp::socket> sock, BookRepository& repo,
                       std::set<std::shared_ptr<tcp::socket>>& sessions,
                       std::mutex& sessions_mutex) {
  try {
    beast::flat_buffer buffer;
    http::request<http::string_body> req;
    http::read(*sock, buffer, req);
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::server, "book-service/0.1");
    handle_request(repo, std::move(req), res);
    http::write(*sock, res);
    beast::error_code ec2;
    sock->shutdown(tcp::socket::shutdown_send, ec2);
  } catch (std::exception const& e) {
    BOOST_LOG_TRIVIAL(error) << "Session error: " << e.what();
  }
  {
    std::scoped_lock lk(sessions_mutex);
    sessions.erase(sock);
  }
}

std::shared_ptr<ServerControl> start_server(unsigned short port, BookRepository& repo, unsigned thread_count) {
  auto ctrl = std::make_shared<ServerControl>();
  ctrl->impl = std::make_shared<ServerControl::Impl>(port);
  ctrl->impl->repo = &repo;
  if (thread_count == 0) thread_count = std::max(1u, std::thread::hardware_concurrency());
  ctrl->impl->thread_count = thread_count;

  auto& impl = *ctrl->impl;
  std::cout << "Listening on port " << impl.acceptor.local_endpoint().port() << "\n";

  auto do_accept = std::make_shared<std::function<void()>>();
  *do_accept = [ctrl, do_accept]() {
    auto impl = ctrl->impl; // keep shared ownership inside callbacks
    impl->acceptor.async_accept([ctrl, do_accept](beast::error_code ec, tcp::socket socket) {
      auto impl = ctrl->impl;
      if (!ec) {
        auto sock = std::make_shared<tcp::socket>(std::move(socket));
        {
          std::scoped_lock lk(impl->sessions_mutex);
          impl->sessions.insert(sock);
        }
        net::post(impl->ioc, [sock, impl]() { do_session(sock, *impl->repo, impl->sessions, impl->sessions_mutex); });
      } else {
        BOOST_LOG_TRIVIAL(error) << "Accept error: " << ec.message();
      }
      if (impl->acceptor.is_open()) (*do_accept)();
    });
  };

  if (impl.acceptor.is_open()) (*do_accept)();

  // Note: signal handling is performed by the blocking wrapper `run_server`
  // so that callers using `start_server` (e.g. tests) can control the server
  // lifecycle programmatically without racing with process signals.

  // Launch worker threads (all run the io_context)
  for (unsigned i = 0; i < impl.thread_count; ++i) {
    impl.workers.emplace_back([&impl]() { try { impl.ioc.run(); } catch (...) {} });
  }

  return ctrl;
}

void ServerControl::stop() {
  if (!impl) return;
  BOOST_LOG_TRIVIAL(info) << "ServerControl::stop called";
  try {
    impl->acceptor.close();
  } catch (...) {}
  std::vector<std::shared_ptr<tcp::socket>> copy;
  {
    std::lock_guard lk(impl->sessions_mutex);
    copy.assign(impl->sessions.begin(), impl->sessions.end());
  }
  for (auto& s : copy) {
    try { beast::error_code ec2; s->shutdown(tcp::socket::shutdown_both, ec2); s->close(ec2); } catch(...) {}
  }
  impl->ioc.stop();
}

void ServerControl::wait() {
  if (!impl) return;
  try {
    for (auto& t : impl->workers) if (t.joinable()) t.join();
  } catch(...) {}
}

void run_server(unsigned short port, BookRepository& repo, unsigned thread_count) {
  auto ctrl = start_server(port, repo, thread_count);
  if (!ctrl) return;
  try {
    // Store the signal_set on the control impl so it lives for the server lifetime.
    ctrl->impl->signals = std::make_unique<net::signal_set>(ctrl->impl->ioc, SIGINT, SIGTERM);
    BOOST_LOG_TRIVIAL(info) << "Registering signal handler";
    ctrl->impl->signals->async_wait([ctrl](beast::error_code ec, int signo) {
      BOOST_LOG_TRIVIAL(info) << "Shutdown requested via signal " << signo << " ec=" << ec.message();
      ctrl->stop();
    });
  } catch (...) {}
  ctrl->wait();
}
