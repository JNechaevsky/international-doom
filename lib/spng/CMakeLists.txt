add_library(spng STATIC EXCLUDE_FROM_ALL
    spng.c spng.h
)

target_include_directories(spng INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(spng PRIVATE SPNG_USE_MINIZ SPNG_STATIC SPNG_SUPPRESS_WARNINGS)
target_compile_definitions(spng INTERFACE SPNG_STATIC)
target_link_libraries(spng PUBLIC miniz::miniz)

add_library(spng::spng ALIAS spng)
