cmake_minimum_required(VERSION 3.10)
project(CBDebugLib C)

set(CMAKE_C_STANDARD 99)

if(MSVC)
    # MSVC equivalent of -Wall -Wextra is /W4
    add_compile_options(/W4)
else()
    # For GCC and Clang
    add_compile_options(-Wall -Wextra -Wsign-conversion -pedantic)
endif()

# Until Fortify builds are supported here
set(SOURCES Debug.c)
file(GLOB PUBLIC_HEADERS "*.h")

add_library(CBDebug STATIC ${SOURCES} ${PUBLIC_HEADERS})

set_target_properties(CBDebug PROPERTIES
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
    DEBUG_POSTFIX "dbg"
)

target_include_directories(CBDebug PUBLIC .)

target_compile_definitions(CBDebug PRIVATE
    $<$<CONFIG:Debug>:DEBUG_OUTPUT>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_options(CBDebug PRIVATE
    $<$<CONFIG:Release>:-O3>
)

install(TARGETS CBDebug
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

include(CTest)

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  add_subdirectory(tests)
endif()
