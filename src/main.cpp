#include "http_server.hpp"
#include "book_repository.hpp"
#include "logging.hpp"
#include <iostream>

int main(int argc, char** argv) {
  init_logging();
  unsigned short port = 8080;
  if (argc > 1) port = static_cast<unsigned short>(std::stoi(argv[1]));
  BookRepository repo;
  run_server(port, repo);
  return 0;
}
