cmake_minimum_required(VERSION 3.14)
project(tiny-js LANGUAGES C CXX)

option(TINYJS_BUILD_APPS "Build tiny-js apps" ON)
option(TINYJS_BUILD_TESTS "Buiild tiny-js tests" ON)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "v")
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) #Optional

set(TINYJS_SRCPATH "${CMAKE_CURRENT_LIST_DIR}")
set(PRIVATE_SOURCES 
"${TINYJS_SRCPATH}/TinyJS.cpp"
"${TINYJS_SRCPATH}/TinyJS_Debug.cpp"
"${TINYJS_SRCPATH}/TinyJS_Functions.cpp"
"${TINYJS_SRCPATH}/TinyJS_MathFunctions.cpp"
"${TINYJS_SRCPATH}/TinyJS_StringFunctions.cpp"
"${TINYJS_SRCPATH}/TinyJS_Date.cpp"
"${TINYJS_SRCPATH}/TinyJS_Threading.cpp"
"${TINYJS_SRCPATH}/pool_allocator.cpp"
)

# Building static library
add_library(tiny-js STATIC ${PRIVATE_SOURCES})

set_target_properties(tiny-js PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(tiny-js PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# std::thread / generators: Linux and some Android NDKs need -pthread.
# Windows/MSVC: Threads::Threads is a no-op.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)
if(Threads_FOUND)
  target_link_libraries(tiny-js PUBLIC Threads::Threads)
endif()

if(TINYJS_BUILD_TESTS)
  add_executable(tiny-js-tests "run_tests.cpp")
  target_link_libraries(tiny-js-tests tiny-js)
endif()

if(TINYJS_BUILD_APPS)
  add_executable(tiny-js-script "Script.cpp")
  target_link_libraries(tiny-js-script tiny-js)
endif()
