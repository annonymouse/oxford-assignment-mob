add_executable(main main.c)
target_link_libraries(main node pthread uv)
add_library(node node.c)
