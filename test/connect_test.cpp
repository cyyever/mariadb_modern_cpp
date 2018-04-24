/*!
 * \file cfg_test.cpp
 *
 * \brief 测试cfg
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "../hdr/mariadb_modern_cpp.hpp"

TEST_CASE("connect with passwd") {
  sqlite::mariadb_config config;
  try {
    config.host = "127.0.0.1";
    config.user = "root";
    config.passwd = "123";
    sqlite::database test_db("mysql", config);
  } catch (const sqlite::mariadb_exception &e) {
    auto err_msg = e.what();
    CHECK_MESSAGE(false, err_msg);
  }
}
