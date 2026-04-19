#pragma once

#include "book_repository.hpp"
#include <boost/asio.hpp>
#include <memory>

// Control handle returned by `start_server` allowing programmatic shutdown.
struct ServerControl {
	struct Impl;
	std::shared_ptr<Impl> impl;
	// Stop the server (close acceptor, close sessions, stop io_context)
	void stop();
	// Wait until server stops (blocks)
	void wait();
};

// Start the server and return a control handle. The server runs asynchronously
// on background worker threads. Use `stop()` to request shutdown.
std::shared_ptr<ServerControl> start_server(unsigned short port, BookRepository& repo, unsigned thread_count = 0);

// Blocking convenience wrapper that starts the server and blocks until shutdown.
void run_server(unsigned short port, BookRepository& repo, unsigned thread_count = 0);
