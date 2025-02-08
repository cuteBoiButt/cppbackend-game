cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

# Для простоты работы с IDE
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Санитайзеры никогда не помешают :з
add_compile_options("$<$<CONFIG:Debug>:-fsanitize=address,undefined>")
add_link_options("$<$<CONFIG:Debug>:-fsanitize=address,undefined>")

add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW) 

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(common_lib STATIC
	src/sdk.h
	src/tagged.h
	src/tagged_uuid.h
	src/tagged_uuid.cpp
	src/extra_data.h
	src/extra_data.cpp
    src/model.h
    src/model.cpp
	src/geom.h
    src/loot_generator.h
    src/loot_generator.cpp
	src/model_serialization.h
	src/model_serialization.cpp
	src/app.cpp
	src/app.h
	src/json_loader.h
	src/json_loader.cpp
	src/boost_json.cpp
	src/collision_detector.cpp
	src/collision_detector.h
)
target_include_directories(common_lib PUBLIC CONAN_PKG::boost)
target_link_libraries(common_lib PUBLIC CONAN_PKG::boost Threads::Threads CONAN_PKG::libpq CONAN_PKG::libpqxx)

add_executable(game_server
	src/main.cpp
	src/http_server.cpp
	src/http_server.h
	src/request_handler.cpp
	src/request_handler.h
	src/log.cpp
	src/log.h
	src/db.h
	src/postgres.cpp
	src/postgres.h
	src/ticker.cpp
	src/ticker.h
	src/retirement.cpp
	src/retirement.h
	src/serialization.h
	src/serialization.cpp
)
target_link_libraries(game_server PRIVATE common_lib)

add_executable(game_server_tests
    tests/loot_generator_tests.cpp
	tests/state-serialization-tests.cpp
)
target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 common_lib)
