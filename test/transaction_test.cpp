/*!
 * \file transaction_test.cpp
 *
 * \author cyy
 * \date 2018-07-06
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cmath>
#include <cstddef>
#include <doctest.h>

#include "../hdr/mariadb_modern_cpp.hpp"

TEST_CASE("transaction") {
  mariadb::mariadb_config config;
  config.host = "127.0.0.1";
  config.user = "mariadb_modern_cpp_test";
  config.passwd = "123";
  config.default_database = "mariadb_modern_cpp_test";
  mariadb::database test_db(config);

  SUBCASE("roll_back") {
    test_db << "CREATE TABLE IF NOT EXISTS mariadb_modern_cpp_test.tmp_table "
               "(id BIGINT PRIMARY KEY AUTO_INCREMENT NOT NULL);";
    test_db << "delete from mariadb_modern_cpp_test.tmp_table;";
    try {
      auto ctx = test_db.get_transaction_context();
      test_db << "INSERT INTO tmp_table VALUES ();";
      size_t cnt = 0;
      test_db << "select count(*) from tmp_table;" >> cnt;
      CHECK(cnt == 1);
      throw std::runtime_error("");
    } catch (...) {
    }
    size_t cnt = 0;
    test_db << "select count(*) from tmp_table;" >> cnt;
    CHECK(cnt == 0);
    test_db << "drop table mariadb_modern_cpp_test.tmp_table;";
  }

  SUBCASE("commit") {
    test_db << "CREATE TABLE IF NOT EXISTS mariadb_modern_cpp_test.tmp_table "
               "(id BIGINT PRIMARY KEY AUTO_INCREMENT NOT NULL);";
    test_db << "delete from mariadb_modern_cpp_test.tmp_table;";
    {
      auto ctx = test_db.get_transaction_context();
      test_db << "INSERT INTO tmp_table VALUES ();";
      size_t cnt = 0;
      test_db << "select count(*) from tmp_table;" >> cnt;
      CHECK(cnt == 1);
    }
    size_t cnt = 0;
    test_db << "select count(*) from tmp_table;" >> cnt;
    CHECK(cnt == 1);
    test_db << "drop table mariadb_modern_cpp_test.tmp_table;";
  }
}
