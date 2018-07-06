/*!
 * \file connect_test.cpp
 *
 * \date 2018-07-06
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "../hdr/mariadb_modern_cpp.hpp"

TEST_CASE("connect with passwd") {
  mariadb::mariadb_config config;
  try {
    config.host = "127.0.0.1";
    config.user = "mariadb_modern_cpp_test";
    config.passwd = "123";
    config.default_database = "mariadb_modern_cpp_test";
    mariadb::database test_db(config);
  } catch (const mariadb::exceptions::connection &e) {
    auto err_msg = e.what();
    CHECK_MESSAGE(false, err_msg);
  }
}

TEST_CASE("connect without passwd") {
  bool has_exception = false;
  try {
    mariadb::mariadb_config config;
    config.host = "127.0.0.1";
    config.user = "mariadb_modern_cpp_test";
    config.default_database = "mariadb_modern_cpp_test";
    mariadb::database test_db(config);
  } catch (const mariadb::exceptions::connection &) {
    has_exception = true;
  }
  CHECK(has_exception);
}
