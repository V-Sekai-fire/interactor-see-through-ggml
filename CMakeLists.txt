cmake_minimum_required(VERSION 3.22)
project(interactor-see-through-cpp CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The harness carries the bus, the limits and the command envelope. It dlopens iceoryx2, so
# this builds on a machine that has never seen it and fails at start rather than at link when
# it is absent. Its own proof programs are somebody else's test and are not built here.
set(WEFT_HARNESS_PROOF OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/harness)

# The interactor itself: the command, the gate and the reply. No bus, no engine, no GPU -- so
# the proof below links this and nothing else, and runs on a laptop.
add_library(seethrough STATIC
    src/command.c
    src/engine_absent.c
    third_party/interactor/src/cbor.c)
target_include_directories(seethrough PUBLIC include third_party/interactor/include)

add_executable(seethrough-interactor src/main.cpp)
target_link_libraries(seethrough-interactor PRIVATE seethrough weft::harness)
target_compile_options(seethrough-interactor PRIVATE -Wall -Wextra)

enable_testing()
add_executable(seethrough-decompose proof/decompose.c)
target_link_libraries(seethrough-decompose PRIVATE seethrough)
target_compile_options(seethrough-decompose PRIVATE -Wall -Wextra)
add_test(NAME decompose COMMAND seethrough-decompose)
