CMAKE_MINIMUM_REQUIRED(VERSION 3.9)

INCLUDE(${CMAKE_CURRENT_LIST_DIR}/../cmake/test.cmake)

FIND_PACKAGE(doctest REQUIRED)

SET(test_progs connect_test select_test insert_test concurrent_test transaction_test)

FOREACH(test_prog ${test_progs})
  ADD_EXECUTABLE(${test_prog} ${CMAKE_CURRENT_LIST_DIR}/${test_prog}.cpp)
  TARGET_LINK_LIBRARIES(${test_prog} PRIVATE mariadb_modern_cpp)
  TARGET_LINK_LIBRARIES(${test_prog} PRIVATE doctest::doctest)
  add_test_with_runtime_analysis(TARGET ${test_prog} HELGRIND TRUE TSAN TRUE)
ENDFOREACH()
