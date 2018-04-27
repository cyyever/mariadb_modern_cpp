CMAKE_MINIMUM_REQUIRED(VERSION 3.9)

INCLUDE(${CMAKE_CURRENT_LIST_DIR}/../cmake/fuzzing.cmake)

SET(test_progs sql_test)

FOREACH(test_prog ${test_progs})
  ADD_EXECUTABLE(${test_prog} ${CMAKE_CURRENT_LIST_DIR}/${test_prog}.cpp)
  TARGET_LINK_LIBRARIES(${test_prog} PRIVATE mariadb_modern_cpp)
  add_fuzzing(TARGET ${test_prog})
ENDFOREACH()
