/*!
 * \file cfg_test.cpp
 *
 * \brief 测试cfg
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "../hdr/mariadb_modern_cpp.hpp"

TEST_CASE("select") {
  sqlite::mariadb_config config;
  config.host = "127.0.0.1";
  config.user = "root";
  config.passwd = "123";
  sqlite::database test_db("mysql", config);

  SUBCASE("select without argument") { test_db << "select * from user;"; }

  SUBCASE("select with arguments") {
    // test_db<<"select * from user where user=?;"<<std::string("root");
    test_db << "select * from user where user=?;"
            << "root";
  }
}
