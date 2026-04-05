add_library(ebur128 STATIC EXCLUDE_FROM_ALL
    ebur128.c
    ebur128.h
    queue/sys/queue.h
)

target_include_directories(ebur128 INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_include_directories(ebur128 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/queue"
)

if(TARGET m)
    target_link_libraries(ebur128 PUBLIC m)
endif()

add_library(ebur128::ebur128 ALIAS ebur128)
