#include <gtest/gtest.h>
#include "book_repository.hpp"
#include "http_server.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

static std::pair<int, std::string> do_request(const std::string& host, unsigned short port,
                                              http::verb method, const std::string& target,
                                              const std::string& body = "") {
  try {
    net::io_context ioc;
    tcp::resolver resolver{ioc};
    auto const results = resolver.resolve(host, std::to_string(port));
    tcp::socket socket{ioc};
    net::connect(socket, results.begin(), results.end());

    http::request<http::string_body> req{method, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, "test-client/0.1");
    if (!body.empty()) {
      req.set(http::field::content_type, "application/json");
      req.body() = body;
      req.prepare_payload();
    }

    http::write(socket, req);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(socket, buffer, res);
    beast::error_code ec;
    socket.shutdown(tcp::socket::shutdown_both, ec);
    return {static_cast<int>(res.result_int()), res.body()};
  } catch (std::exception const& e) {
    return {-1, std::string("error: ") + e.what()};
  }
}

TEST(Integration, Concurrency) {
  unsigned short port = 18081;
  BookRepository repo;

  std::thread server_thread([&](){ run_server(port, repo, 4); });
  server_thread.detach();

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  const int clients = 8;
  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};

  for (int i = 0; i < clients; ++i) {
    threads.emplace_back([&, i]() {
      std::string book_json = "{\"title\":\"Book" + std::to_string(i) + "\",\"author\":\"Author\",\"year\":2000}";
      auto [s, b] = do_request("127.0.0.1", port, http::verb::post, "/books", book_json);
      if (s == 201) success_count.fetch_add(1);
    });
  }

  for (auto& t : threads) if (t.joinable()) t.join();

  EXPECT_EQ(success_count.load(), clients);

  // confirm count
  auto [s2, b2] = do_request("127.0.0.1", port, http::verb::get, "/books");
  EXPECT_EQ(s2, 200);
  for (int i = 0; i < clients; ++i) EXPECT_NE(b2.find("Book" + std::to_string(i)), std::string::npos);
}
