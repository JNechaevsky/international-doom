add_library(stb_image_write INTERFACE)
target_include_directories(stb_image_write INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
add_library(stb::image_write ALIAS stb_image_write)
