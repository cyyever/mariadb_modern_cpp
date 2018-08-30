/*!
 * \file insert_test.cpp
 *
 * \date 2018-07-06
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cmath>
#include <cstddef>
#include <doctest.h>

#include "../hdr/mariadb_modern_cpp.hpp"
#include "test_config.hpp"

TEST_CASE("insert") {
  mariadb::database test_db(get_test_config());

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

  SUBCASE("batch insert") {

    test_db << "CREATE TABLE IF NOT EXISTS mariadb_modern_cpp_test.tmp_table "
               "(id BIGINT PRIMARY KEY AUTO_INCREMENT NOT NULL);";
    auto ps =
        test_db << "insert into mariadb_modern_cpp_test.tmp_table values (?)";
    int i = 1;
    while (i < 100) {
      ps << i;
      ps.execute();
      i++;
    }
    test_db << "drop TABLE mariadb_modern_cpp_test.tmp_table;";
  }
}
