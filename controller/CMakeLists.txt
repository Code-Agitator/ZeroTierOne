cmake_minimum_required(VERSION 3.13)
project(zerotier-controller)


set(SRC_FILES
    DB.cpp
    DB.hpp
    DBMirrorSet.cpp
    DBMirrorSet.hpp
    EmbeddedNetworkController.cpp
    EmbeddedNetworkController.hpp
    FileDB.cpp
    FileDB.hpp
    LFDB.cpp
    LFDB.hpp
    CtlUtil.cpp
    CtlUtil.hpp
)

set(INCLUDE_DIRS
    "${httplib_SOURCE_DIR}"
    ${RUSTYBITS_INCLUDE_DIR}
)

set(LINK_LIBS
    zerotier-osdep
    nlohmann_json::nlohmann_json 
    opentelemetry-cpp::api
    rustybits
    Threads::Threads
    prometheus-cpp-lite
    Threads::Threads
)

if (ZT1_CENTRAL_CONTROLLER)
    find_package(PostgreSQL REQUIRED)

    list(APPEND SRC_FILES
        CV1.cpp
        CV1.hpp
        CV2.cpp
        CV2.hpp
        CentralDB.cpp
        CentralDB.hpp
        NotificationListener.hpp
        PostgreSQL.cpp
        PostgreSQL.hpp
        PubSubListener.cpp
        PubSubListener.hpp
        Redis.hpp
        RedisListener.cpp
        RedisListener.hpp)

    list(APPEND INCLUDE_DIRS
        ${PostgreSQL_INCLUDE_DIRS}
        "${redis++_BUILD_DIR}/src"
        ${pqxx_INCLUDE_DIRS}
    )

    list(APPEND LINK_LIBS
        redis++::redis++_static
        pqxx
        ${PostgreSQL_LIBRARIES}
    )
endif()

add_library(zerotier-controller STATIC ${SRC_FILES})

target_include_directories(zerotier-controller PRIVATE ${INCLUDE_DIRS})

add_dependencies(zerotier-controller redis++::redis++)

target_link_libraries(zerotier-controller 
    ${LINK_LIBS}
)
