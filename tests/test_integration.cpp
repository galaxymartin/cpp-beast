#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <thread>
#include <chrono>
#include <string>

#include "book_repository.hpp"
#include "http_server.hpp"

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

TEST(Integration, SimpleCRUD) {
  unsigned short port = 18080;
  BookRepository repo; // in-memory repo

  // start server (non-blocking) so test can perform requests
  auto ctrl = start_server(port, repo);
  ASSERT_TRUE(ctrl);

  // wait briefly for server to start
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // initial GET /books -> expect empty array
  auto [s0, b0] = do_request("127.0.0.1", port, http::verb::get, "/books");
  EXPECT_EQ(s0, 200);
  EXPECT_TRUE(b0 == "[]" || b0 == "[]\n" || b0.find('[') != std::string::npos);

  // POST /books -> create
  std::string book_json = "{\"title\":\"It\",\"author\":\"King\",\"year\":1986}";
  auto [s1, b1] = do_request("127.0.0.1", port, http::verb::post, "/books", book_json);
  EXPECT_EQ(s1, 201);
  EXPECT_NE(b1.find("id"), std::string::npos);

  // GET /books -> now contains at least one
  auto [s2, b2] = do_request("127.0.0.1", port, http::verb::get, "/books");
  EXPECT_EQ(s2, 200);
  EXPECT_NE(b2.find("It"), std::string::npos);

  // parse id from creation response (simple parse)
  size_t idpos = b1.find("\"id\"");
  uint64_t id = 0;
  if (idpos != std::string::npos) {
    auto colon = b1.find(':', idpos);
    if (colon != std::string::npos) {
      auto comma = b1.find_first_of(",}\n", colon+1);
      std::string idstr = b1.substr(colon+1, (comma==std::string::npos?b1.size():comma)-colon-1);
      try { id = std::stoull(idstr); } catch(...) { id = 0; }
    }
  }
  ASSERT_GT(id, 0u);

  // GET /books/{id}
  auto [s3, b3] = do_request("127.0.0.1", port, http::verb::get, "/books/" + std::to_string(id));
  EXPECT_EQ(s3, 200);
  EXPECT_NE(b3.find("It"), std::string::npos);

  // PUT /books/{id}
  std::string update = "{\"title\":\"It (Updated)\",\"author\":\"Stephen\",\"year\":1986}";
  auto [s4, b4] = do_request("127.0.0.1", port, http::verb::put, "/books/" + std::to_string(id), update);
  EXPECT_EQ(s4, 200);
  EXPECT_NE(b4.find("It (Updated)"), std::string::npos);

  // DELETE /books/{id}
  auto [s5, b5] = do_request("127.0.0.1", port, http::verb::delete_, "/books/" + std::to_string(id));
  EXPECT_EQ(s5, 204);

  // GET /books/{id} now 404
  auto [s6, b6] = do_request("127.0.0.1", port, http::verb::get, "/books/" + std::to_string(id));
  EXPECT_EQ(s6, 404);

  ctrl->stop();
  ctrl->wait();
}
