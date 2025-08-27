cmake_minimum_required(VERSION 3.13)
project(service)

file(GLOB service_src_glob ${PROJ_DIR}/service/*.cpp)
file(GLOB service_hdr_glob ${PROJ_DIR}/service/*.hpp)

add_library(zerotier-service STATIC ${service_src_glob} ${service_hdr_glob})

set(INCLUDE_DIRS
    "${httplib_SOURCE_DIR}"
    ${RUSTYBITS_INCLUDE_DIR}
)

find_package(OpenSSL REQUIRED)

set(LINK_LIBS
    zerotier-osdep
    zerotier-core
    prometheus-cpp-lite
    nlohmann_json::nlohmann_json 
    Threads::Threads
    opentelemetry-cpp::api
    Threads::Threads
    OpenSSL::Crypto
    OpenSSL::SSL
    rustybits
)

if(ZT1_CENTRAL_CONTROLLER)
    add_definitions(-DZT_CONTROLLER_USE_LIBPQ)
    list(APPEND INCLUDE_DIRS
        ${PostgreSQL_INCLUDE_DIRS}
        "${redis++_BUILD_DIR}/src"
        ${pqxx_INCLUDE_DIRS}
    )
    list(APPEND LINK_LIBS
        pqxx
        opentelemetry-cpp::sdk
    )
endif()

target_include_directories(zerotier-service
    PRIVATE
        ${INCLUDE_DIRS}
)


target_link_libraries(zerotier-service 
    ${LINK_LIBS}
)
