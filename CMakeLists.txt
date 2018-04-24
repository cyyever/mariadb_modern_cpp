CMAKE_MINIMUM_REQUIRED(VERSION 3.9)

PROJECT(mariadb_modern_cpp)

INCLUDE(cmake/all.cmake)

FIND_PACKAGE(doctest REQUIRED)

add_library(mariadb_modern_cpp INTERFACE)

target_include_directories(mariadb_modern_cpp INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hdr>
    $<INSTALL_INTERFACE:include>
)

TARGET_LINK_LIBRARIES(mariadb_modern_cpp INTERFACE 
  mariadb
)

# test
add_subdirectory(test)

# install lib
INSTALL(TARGETS mariadb_modern_cpp EXPORT ${PROJECT_NAME}Targets
  )

install(DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/hdr/
  DESTINATION include
)


# install cmake config file
INSTALL(EXPORT ${PROJECT_NAME}Targets
  FILE ${PROJECT_NAME}Config.cmake
  DESTINATION lib/cmake/${PROJECT_NAME})
