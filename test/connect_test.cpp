/*!
 * \file connect_test.cpp
 *
 * \date 2018-07-06
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "../hdr/mariadb_modern_cpp.hpp"
#include "test_config.hpp"

TEST_CASE("connect with passwd") {
  try {
    mariadb::database test_db(get_test_config());
  } catch (const mariadb::exceptions::connection &e) {
    auto err_msg = e.what();
    CHECK_MESSAGE(false, err_msg);
  }
}

TEST_CASE("connect without passwd") {
  bool has_exception = false;
  try {
    auto config = get_test_config();
    config.passwd.clear();
    mariadb::database test_db(config);
  } catch (const mariadb::exceptions::connection &) {
    has_exception = true;
  }
  CHECK(has_exception);
}
