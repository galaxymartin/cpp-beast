#include "http_server.hpp"
#include "book.hpp"
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <boost/log/trivial.hpp>
#include <set>
#include <mutex>
#include <charconv>
#include <string_view>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

static void send_json_error(http::response<http::string_body>& res, http::status status, std::string_view message) {
  boost::json::object obj;
  obj["error"] = std::string(message);
  res.result(status);
  res.set(http::field::content_type, "application/json");
  res.body() = boost::json::serialize(obj);
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
      res.result(http::status::ok);
      res.set(http::field::content_type, "application/json");
      res.body() = boost::json::serialize(arr);
      res.prepare_payload();
      return;
    }

    // GET /books/{id}
    if (req.method() == http::verb::get) {
      std::string_view target{req.target()};
      if (target.rfind("/books/", 0) == 0) {
        auto idstr = target.substr(std::strlen("/books/"));
        uint64_t id = 0;
        int64_t tmp = 0;
        auto parse_res = std::from_chars(idstr.data(), idstr.data() + idstr.size(), tmp);
        if (parse_res.ec != std::errc() || parse_res.ptr == idstr.data() || tmp < 0) {
          send_json_error(res, http::status::bad_request, "invalid id");
          return;
        }
        id = static_cast<uint64_t>(tmp);
        auto ob = repo.get(id);
        if (!ob) { BOOST_LOG_TRIVIAL(info) << "Book not found id=" << id; send_json_error(res, http::status::not_found, "book not found"); return; }
        boost::json::value jv; to_json(jv, *ob);
        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = boost::json::serialize(jv);
        res.prepare_payload();
        return;
      }
    }

    // POST /books
    if (req.method() == http::verb::post && req.target() == "/books") {
      boost::json::value jv;
      try { jv = boost::json::parse(req.body()); } catch (std::exception const&) { send_json_error(res, http::status::bad_request, "invalid json"); return; }
      std::string verr;
      if (!validate_book_for_create(jv, verr)) { send_json_error(res, http::status::bad_request, verr); return; }
      Book b; from_json(jv, b);
      auto added = repo.add(b);
      boost::json::value out; to_json(out, added);
      BOOST_LOG_TRIVIAL(info) << "Book created id=" << added.id;
      res.result(http::status::created);
      res.set(http::field::content_type, "application/json");
      res.body() = boost::json::serialize(out);
      res.prepare_payload();
      return;
    }

    // PUT /books/{id}
    if (req.method() == http::verb::put) {
      std::string_view target{req.target()};
      if (target.rfind("/books/", 0) == 0) {
        auto idstr = target.substr(std::strlen("/books/"));
        uint64_t id = 0;
        int64_t tmp = 0;
        auto parse_res = std::from_chars(idstr.data(), idstr.data() + idstr.size(), tmp);
        if (parse_res.ec != std::errc() || parse_res.ptr == idstr.data() || tmp < 0) {
          send_json_error(res, http::status::bad_request, "invalid id");
          return;
        }
        id = static_cast<uint64_t>(tmp);
        boost::json::value jv;
        try { jv = boost::json::parse(req.body()); } catch (std::exception const&) { send_json_error(res, http::status::bad_request, "invalid json"); return; }
        Book b; from_json(jv, b);
        auto updated = repo.update(id, b);
        if (!updated) { BOOST_LOG_TRIVIAL(info) << "Book update not found id=" << id; send_json_error(res, http::status::not_found, "book not found"); return; }
        boost::json::value out; to_json(out, *updated);
        BOOST_LOG_TRIVIAL(info) << "Book updated id=" << id;
        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = boost::json::serialize(out);
        res.prepare_payload();
        return;
      }
    }

    // DELETE /books/{id}
    if (req.method() == http::verb::delete_) {
      std::string_view target{req.target()};
      if (target.rfind("/books/", 0) == 0) {
        auto idstr = target.substr(std::strlen("/books/"));
        uint64_t id = 0;
        int64_t tmp = 0;
        auto parse_res = std::from_chars(idstr.data(), idstr.data() + idstr.size(), tmp);
        if (parse_res.ec != std::errc() || parse_res.ptr == idstr.data() || tmp < 0) {
          send_json_error(res, http::status::bad_request, "invalid id");
          return;
        }
        id = static_cast<uint64_t>(tmp);
        auto removed = repo.remove(id);
        BOOST_LOG_TRIVIAL(info) << "Book delete id=" << id << " removed=" << removed;
        if (!removed) { send_json_error(res, http::status::not_found, "book not found"); return; }
        res.result(http::status::no_content);
        res.prepare_payload();
        return;
      }
    }

    // Not found
    send_json_error(res, http::status::not_found, "not found");
    return;
  } catch (std::exception const& e) {
    BOOST_LOG_TRIVIAL(error) << "HTTP handler exception: " << e.what();
    send_json_error(res, http::status::internal_server_error, std::string("error: ") + e.what());
    return;
  }
}

void run_server(unsigned short port, BookRepository& repo, unsigned thread_count) {
  try {
    net::io_context ioc{1};
    tcp::acceptor acceptor{ioc, tcp::endpoint{tcp::v4(), port}};
    std::cout << "Listening on port " << port << "\n";

    if (thread_count == 0) {
      thread_count = std::max(1u, std::thread::hardware_concurrency());
    }

    // Accept loop: async accept sockets and post synchronous session handlers to the io_context.
    std::function<void()> do_accept;
    // track active sessions so we can close them on shutdown
    std::mutex sessions_mutex;
    std::set<std::shared_ptr<tcp::socket>> sessions;

    do_accept = [&]() {
      acceptor.async_accept([&](beast::error_code ec, tcp::socket socket) {
        if (!ec) {
          auto sock = std::make_shared<tcp::socket>(std::move(socket));
          {
            std::scoped_lock lk(sessions_mutex);
            sessions.insert(sock);
          }
          // Post session handling onto the io_context so worker threads execute it.
          net::post(ioc, [sock, &repo, &sessions, &sessions_mutex]() {
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
          });
        } else {
          BOOST_LOG_TRIVIAL(error) << "Accept error: " << ec.message();
        }
        if (acceptor.is_open()) do_accept();
      });
    };

    if (acceptor.is_open()) do_accept();

    // Setup signal handling for graceful shutdown (Ctrl+C)
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](beast::error_code /*ec*/, int /*signo*/) {
      BOOST_LOG_TRIVIAL(info) << "Shutdown requested via signal";
      try {
        acceptor.close();
      } catch (...) {}

      // Close active sessions to unblock any blocked reads/writes
      std::vector<std::shared_ptr<tcp::socket>> copy;
      {
        std::lock_guard lk(sessions_mutex);
        copy.assign(sessions.begin(), sessions.end());
      }
      BOOST_LOG_TRIVIAL(info) << "Closing " << copy.size() << " active sessions";
      for (auto& s : copy) {
        try {
          beast::error_code ec2;
          s->shutdown(tcp::socket::shutdown_both, ec2);
          s->close(ec2);
        } catch (...) {}
      }

      ioc.stop();
    });

    // Launch worker threads
    std::vector<std::thread> workers;
    for (unsigned i = 1; i < thread_count; ++i) {
      workers.emplace_back([&ioc]() { try { ioc.run(); } catch (...) {} });
    }

    // Run on this thread as well (blocks)
    try { ioc.run(); } catch (...) {}

    // join workers on shutdown
    for (auto& t : workers) if (t.joinable()) t.join();
    BOOST_LOG_TRIVIAL(info) << "Server stopped";
  } catch (std::exception const& e) {
    std::cerr << "Server error: " << e.what() << std::endl;
  }
}
