set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
    redis-plus-plus
    GIT_REPOSITORY https://github.com/sewenew/redis-plus-plus.git
    GIT_TAG 1.3.15
    GIT_SHALLOW ON
)
set(REDIS_PLUS_PLUS_BUILD_STATIC ON CACHE INTERNAL "Build static library" FORCE)
set(REDIS_PLUS_PLUS_BUILD_SHARED ON CACHE INTERNAL "Build shared library" FORCE)
set(REDIS_PLUS_PLUS_BUILD_TEST OFF CACHE INTERNAL "Build tests" FORCE)
set(REDIS_PLUS_PLUS_BUILD_STATIC_WITH_PIC ON CACHE INTERNAL "Build static library with PIC" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "Build shared libraries" FORCE)

FetchContent_MakeAvailable(redis-plus-plus)
if(NOT TARGET redis++::redis++_static)
    message(FATAL_ERROR "A required redis-plus-plus target (redis++::redis++) was not imported")
endif()
message(STATUS "redis-plus-plus imported")
