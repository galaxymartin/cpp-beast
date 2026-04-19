#include <gtest/gtest.h>
#include "http_server.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <thread>

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

TEST(HttpServer, PostInvalidJsonReturns400) {
  unsigned short port = 18100;
  BookRepository repo;
  auto ctrl = start_server(port, repo, 1);
  ASSERT_TRUE(ctrl);

  // malformed JSON
  auto [s, b] = do_request("127.0.0.1", port, http::verb::post, "/books", "{not json}");
  EXPECT_EQ(s, 400);

  ctrl->stop();
  ctrl->wait();
}
