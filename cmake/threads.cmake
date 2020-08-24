# threads.cmake
# Cross-platform thread linking.
#
# Creates an interface library target `threads` that
# specifies the appropriate threading library to link
# when linked against via target_link_libraries().

find_package(Threads)

add_library(threads INTERFACE)
target_link_libraries(threads INTERFACE ${CMAKE_THREAD_LIBS_INIT})