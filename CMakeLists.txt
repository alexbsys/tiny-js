project(tiny-js LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "v")
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) #Optional

file(GLOB PRIVATE_SOURCES *.cpp *.h)


set(TINYJS_SRCPATH "${CMAKE_CURRENT_LIST_DIR}")

set(PRIVATE_SOURCES 
"${TINYJS_SRCPATH}/TinyJS.cpp"
"${TINYJS_SRCPATH}/TinyJS_Functions.cpp"
"${TINYJS_SRCPATH}/TinyJS_MathFunctions.cpp"
"${TINYJS_SRCPATH}/TinyJS_StringFunctions.cpp"
"${TINYJS_SRCPATH}/TinyJS_Threading.cpp"
"${TINYJS_SRCPATH}/Script.cpp"
"${TINYJS_SRCPATH}/pool_allocator.cpp"
)

# Building static library
add_library(tiny-js STATIC
    ${PRIVATE_SOURCES}
  )

#  target_compile_definitions(ffmpeg-loader PUBLIC FFMPEG_LOADER_DLL=0)
#  target_sources(ffmpeg-loader PRIVATE ${EXTERNAL_SOURCES})

set_target_properties(tiny-js PROPERTIES POSITION_INDEPENDENT_CODE ON)
#endif() # FFMPEGLOADER_BUILD_SHARED

target_include_directories(tiny-js PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(tiny-js-tests "run_tests.cpp")
target_link_libraries(tiny-js-tests tiny-js)

add_executable(tiny-js-script "Script.cpp")
target_link_libraries(tiny-js-script tiny-js)
