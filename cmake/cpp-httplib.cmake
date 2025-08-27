
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
    cpp-httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.25.0
    GIT_SHALLOW ON
)
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
set(HTTPLIB_COMPILE OFF CACHE INTERNAL "")
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE ON CACHE INTERNAL "Use zlib if available")
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE ON CACHE INTERNAL "Use brotli if available")
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE ON CACHE INTERNAL "Use OpenSSL if available")
set(HTTPLIB_USE_ZSTD_IF_AVAILABLE ON CACHE INTERNAL "Use zstd if available")
FetchContent_MakeAvailable(cpp-httplib)

if (NOT TARGET httplib::httplib)
     message(FATAL_ERROR "A required cpp-httplib target (cpp-httplib) was not imported")
endif()

