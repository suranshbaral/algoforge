cmake_minimum_required(VERSION 3.20)
project(algoforge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

include_directories(include)
include_directories(${ZMQ_INCLUDE_DIRS})

# Main engine
add_executable(algoforge src/main.cpp)
target_link_libraries(algoforge
    ${ZMQ_LINK_LIBRARIES}
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

# Backtester
add_executable(backtest src/backtest.cpp)
target_link_libraries(backtest
    ${ZMQ_LINK_LIBRARIES}
    CURL::libcurl
    nlohmann_json::nlohmann_json
)
add_executable(backtest_nokia src/backtest_nokia.cpp)
target_link_libraries(backtest_nokia
    ${ZMQ_LINK_LIBRARIES}
    CURL::libcurl
    nlohmann_json::nlohmann_json
)
add_executable(nokia_stats src/nokia_stats.cpp)
target_link_libraries(nokia_stats
    ${ZMQ_LINK_LIBRARIES}
    CURL::libcurl
    nlohmann_json::nlohmann_json
)