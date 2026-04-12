#include "http_server.hpp"
#include "book_repository.hpp"
#include "logging.hpp"
#include <iostream>
#include <charconv>
#include <cstring>
#include <limits>

int main(int argc, char** argv) {
  init_logging();
  unsigned short port = 8080;
  if (argc > 1) {
    int val = 0;
    const char* s = argv[1];
    auto last = s + std::strlen(s);
    auto res = std::from_chars(s, last, val);
    if (res.ec != std::errc() || res.ptr == s) {
      std::cerr << "Invalid port: " << argv[1] << '\n';
      return 1;
    }
    if (val < 0 || val > static_cast<int>(std::numeric_limits<unsigned short>::max())) {
      std::cerr << "Port out of range: " << val << '\n';
      return 1;
    }
    port = static_cast<unsigned short>(val);
  }
  BookRepository repo;
  run_server(port, repo);
  return 0;
}
