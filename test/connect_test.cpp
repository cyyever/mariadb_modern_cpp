/*!
 * \file cfg_test.cpp
 *
 * \brief 测试cfg
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
    mariadb::database test_db("mariadb_modern_cpp_test", config);
  } catch (const mariadb::mariadb_exception &e) {
    auto err_msg = e.what();
    CHECK_MESSAGE(false, err_msg);
  }
}
