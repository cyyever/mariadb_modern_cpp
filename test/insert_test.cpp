/*!
 * \file cfg_test.cpp
 *
 * \brief 测试cfg
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cmath>
#include <cstddef>
#include <doctest.h>

#include "../hdr/mariadb_modern_cpp.hpp"

TEST_CASE("insert") {
  mariadb::mariadb_config config;
  config.host = "127.0.0.1";
  config.user = "mariadb_modern_cpp_test";
  config.passwd = "123";
  config.default_database = "mariadb_modern_cpp_test";
  mariadb::database test_db(config);

  SUBCASE("insert_id") {
    test_db << "CREATE TABLE IF NOT EXISTS mariadb_modern_cpp_test.tmp_table "
               "(id BIGINT PRIMARY KEY AUTO_INCREMENT NOT NULL);";
    test_db << "INSERT INTO tmp_table VALUES ();";
    auto row_id = test_db.insert_id();
    test_db << "INSERT INTO tmp_table VALUES ();";

    auto row_id2 = test_db.insert_id();
    CHECK(row_id2 == row_id + 1);
    test_db << "drop TABLE mariadb_modern_cpp_test.tmp_table;";
  }
}
