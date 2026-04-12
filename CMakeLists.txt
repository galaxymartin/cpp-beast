cmake_minimum_required(VERSION 3.31)
project(cpp-playground VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
enable_testing()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)

option(ENABLE_ADDRESS_SANITIZER "Build with AddressSanitizer for leak/memory checking" OFF)
option(USE_MOLD "Use mold linker by setting CMAKE_LINKER_TYPE" ON)

if(USE_MOLD)
  set(CMAKE_LINKER_TYPE MOLD)
endif()

# Default build type for single-config generators
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)
find_package(OpenMP QUIET)
find_package(GTest CONFIG QUIET)
find_package(PkgConfig)
find_package(boost_log CONFIG QUIET)
find_package(boost_log_setup CONFIG QUIET)
find_package(Boost REQUIRED COMPONENTS system beast log regex)
find_package(boost_json CONFIG QUIET)

include(GoogleTest)

set(SANITIZE_CXX_FLAGS -fsanitize=address -fno-omit-frame-pointer -g -O1)
set(SANITIZE_LINK_FLAGS -fsanitize=address)

add_library(book_service STATIC
  src/book_repository.cpp
  src/book.cpp
  src/logging.cpp
  src/http_server.cpp
)
target_include_directories(book_service PUBLIC src)
target_compile_features(book_service PUBLIC cxx_std_20)

# Link basic deps per-target
target_link_libraries(book_service PRIVATE Threads::Threads)

# Prefer imported Boost targets when available
if(TARGET Boost::system)
  if(TARGET Boost::system)
    target_link_libraries(book_service PRIVATE Boost::system)
  endif()
  if(TARGET Boost::beast)
    target_link_libraries(book_service PRIVATE Boost::beast)
  endif()
  if(TARGET Boost::json)
    target_link_libraries(book_service PRIVATE Boost::json)
  endif()
  if(TARGET Boost::log)
    target_link_libraries(book_service PRIVATE Boost::log)
  endif()
  if(TARGET Boost::log_setup)
    target_link_libraries(book_service PRIVATE Boost::log_setup)
  endif()  
else()
  target_link_libraries(book_service PRIVATE ${Boost_LIBRARIES})
  target_include_directories(book_service PRIVATE ${Boost_INCLUDE_DIRS})
endif()

# Apply sanitizer per-target if requested
target_compile_options(book_service PRIVATE
  "$<$<BOOL:${ENABLE_ADDRESS_SANITIZER}>:${SANITIZE_CXX_FLAGS}>"
)
target_link_options(book_service PRIVATE
  "$<$<BOOL:${ENABLE_ADDRESS_SANITIZER}>:${SANITIZE_LINK_FLAGS}>"
)

add_executable(server src/main.cpp src/http_server.cpp)
target_compile_features(server PRIVATE cxx_std_20)
target_link_libraries(server PRIVATE book_service)
if(OpenMP_CXX_FOUND)
  target_link_libraries(server PRIVATE OpenMP::OpenMP_CXX)
endif()
if(TARGET Boost::system)
  if(TARGET Boost::system)
    target_link_libraries(server PRIVATE Boost::system)
  endif()
  if(TARGET Boost::beast)
    target_link_libraries(server PRIVATE Boost::beast)
  endif()
  if(TARGET Boost::json)
    target_link_libraries(server PRIVATE Boost::json)
  endif()
  if(TARGET Boost::log)
    target_link_libraries(server PRIVATE Boost::log)
  endif()
  if(TARGET Boost::log_setup)
    target_link_libraries(server PRIVATE Boost::log_setup)
  endif()  
endif()

add_executable(tests tests/test_repository.cpp tests/test_repository_update.cpp tests/test_persistence.cpp tests/test_integration.cpp tests/test_integration_concurrency.cpp tests/test_http_utils.cpp)
target_compile_features(tests PRIVATE cxx_std_20)
target_link_libraries(tests PRIVATE book_service)
if(TARGET GTest::gtest_main)
  target_link_libraries(tests PRIVATE GTest::gtest_main)
elseif(TARGET GTest::gmock_main)
  target_link_libraries(tests PRIVATE GTest::gmock_main)
endif()

if(TARGET Boost::system)
  if(TARGET Boost::system)
    target_link_libraries(tests PRIVATE Boost::system)
  endif()
  if(TARGET Boost::beast)
    target_link_libraries(tests PRIVATE Boost::beast)
  endif()
  if(TARGET Boost::json)
    target_link_libraries(tests PRIVATE Boost::json)
  endif()
  if(TARGET Boost::log)
    target_link_libraries(tests PRIVATE Boost::log)
  endif()
  if(TARGET Boost::log_setup)
    target_link_libraries(tests PRIVATE Boost::log_setup)
  endif()  
else()
  target_link_libraries(tests PRIVATE ${Boost_LIBRARIES})
  target_include_directories(tests PRIVATE ${Boost_INCLUDE_DIRS})
endif()

gtest_discover_tests(tests)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
