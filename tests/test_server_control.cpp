#include <gtest/gtest.h>
#include "http_server.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <thread>

namespace net = boost::asio;

TEST(ServerControl, StartStopWait) {
  unsigned short port = 18102;
  BookRepository repo;
  auto ctrl = start_server(port, repo, 1);
  ASSERT_TRUE(ctrl);

  // Stop and wait; server should shut down cleanly
  ctrl->stop();
  ctrl->wait();

  // After stop, attempting to connect should fail (connection refused or similar)
  using namespace boost::asio;
  io_context ioc;
  ip::tcp::resolver resolver{ioc};
  boost::system::error_code ec;
  auto endpoints = resolver.resolve("127.0.0.1", std::to_string(port), ec);
  if (ec) {
    SUCCEED();
    return;
  }
  ip::tcp::socket sock{ioc};
  net::connect(sock, endpoints.begin(), endpoints.end(), ec);
  EXPECT_TRUE(ec || !sock.is_open());
}
