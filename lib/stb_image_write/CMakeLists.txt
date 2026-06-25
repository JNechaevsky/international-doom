add_library(stb_image_write INTERFACE)
target_include_directories(stb_image_write INTERFACE "${CMAKE_SOURCE_DIR}/lib/stb_image_write")
add_library(stb::image_write ALIAS stb_image_write)
