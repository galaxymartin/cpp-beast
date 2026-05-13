#include <iostream>
#include <charconv>
#include <cstring>
#include <limits>
#include <fstream>
#include <string>
#include <cstdint>
#if defined(__linux__)
#include <unistd.h>
#endif
#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_host.h>
#endif
#if defined(_WIN32)
#include <windows.h>
#endif

#include "http_server.hpp"
#include "book_repository.hpp"
#include "logging.hpp"

int main(int argc, char **argv)
{
  init_logging();
  // Print available memory using a small cross-platform wrapper
  auto get_available_memory_bytes = []() -> uint64_t
  {
#if defined(_WIN32)
    MEMORYSTATUSEX st;
    st.dwLength = sizeof(st);
    if (GlobalMemoryStatusEx(&st))
      return static_cast<uint64_t>(st.ullAvailPhys);
    return 0;
#elif defined(__linux__)
  // Try sysconf first
#if defined(_SC_AVPHYS_PAGES) && defined(_SC_PAGESIZE)
    long pages = sysconf(_SC_AVPHYS_PAGES);
    long page_size = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page_size > 0)
      return static_cast<uint64_t>(pages) * static_cast<uint64_t>(page_size);
#endif
    // Fallback to parsing /proc/meminfo for MemAvailable (kB)
    std::ifstream f("/proc/meminfo");
    if (!f)
      return 0;
    std::string key;
    unsigned long long value = 0;
    std::string unit;
    while (f >> key >> value >> unit)
    {
      if (key == "MemAvailable:")
        return value * 1024ULL;
    }
    return 0;
#elif defined(__APPLE__)
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t vmstat;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vmstat), &count) == KERN_SUCCESS)
    {
      int64_t free_count = static_cast<int64_t>(vmstat.free_count) + static_cast<int64_t>(vmstat.inactive_count);
      vm_size_t page_size = 0;
      host_page_size(mach_host_self(), &page_size);
      if (free_count > 0 && page_size > 0)
        return static_cast<uint64_t>(free_count) * static_cast<uint64_t>(page_size);
    }
    return 0;
#else
    (void)0;
    return 0;
#endif
  };

  uint64_t mem_bytes = get_available_memory_bytes();
  if (mem_bytes)
  {
    uint64_t mem_kb = mem_bytes / 1024ULL;
    std::cout << "Available memory: " << (mem_kb / 1024ULL) << " MB (" << mem_kb << " kB)" << '\n';
  }
  else
  {
    std::cout << "Available memory: unknown" << '\n';
  }
  
  unsigned short port = 8080;
  if (argc > 1)
  {
    int val = 0;
    const char *s = argv[1];
    auto last = s + std::strlen(s);
    auto res = std::from_chars(s, last, val);
    if (res.ec != std::errc() || res.ptr == s)
    {
      std::cerr << "Invalid port: " << argv[1] << '\n';
      return 1;
    }
    if (val < 0 || val > static_cast<int>(std::numeric_limits<unsigned short>::max()))
    {
      std::cerr << "Port out of range: " << val << '\n';
      return 1;
    }
    port = static_cast<unsigned short>(val);
  }
  BookRepository repo;
  run_server(port, repo);
  return 0;
}
