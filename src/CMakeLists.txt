add_executable(main main.c)
target_link_libraries(main uv)

add_executable(node node.c)
target_link_libraries(node uv)
