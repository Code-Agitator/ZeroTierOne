cmake_minimum_required(VERSION 3.13)
project(osdep LANGUAGES CXX ASM)

set(SRC_FILES
    Arp.cpp
    Arp.hpp
    Binder.hpp
    BlockingQueue.hpp
    EthernetTap.cpp
    EthernetTap.hpp
    Http.cpp
    Http.hpp
    ManagedRoute.cpp
    ManagedRoute.hpp
    NeighborDiscovery.cpp
    NeighborDiscovery.hpp
    OSUtils.cpp
    OSUtils.hpp
    Phy.hpp
    PortMapper.cpp
    PortMapper.hpp
    Thread.hpp
)

option(EXT_OSDEP "Build with external osdep feature" OFF)

if(EXT_OSDEP)
    add_definitions(-DZT_EXTOSDEP)
    list(APPEND SRC_FILES
        ExtOsdep.cpp
        ExtOsdep.hpp)
endif()

if(LINUX)
    list(APPEND SRC_FILES
        LinuxEthernetTap.cpp
        LinuxEthernetTap.hpp
        LinuxNetLink.cpp
        LinuxNetLink.hpp)
endif()

if(MACOS)
    list(APPEND SRC_FILES
        MacOSEthernetTap.cpp
        MacOSEthernetTap.hpp
        MacKextEthernetTap.cpp
        MacKextEthernetTap.hpp)
endif()

add_library(zerotier-osdep STATIC ${SRC_FILES})
target_link_libraries(zerotier-osdep
    PRIVATE 
        nlohmann_json::nlohmann_json
        Threads::Threads
        prometheus-cpp-lite
        Threads::Threads
        miniupnpc::miniupnpc
        natpmp
)
