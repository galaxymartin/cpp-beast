#pragma once

#include "book_repository.hpp"
#include <boost/asio.hpp>

void run_server(unsigned short port, BookRepository& repo, unsigned thread_count = 0);
