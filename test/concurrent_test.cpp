/*!
 * \file concurrent_test.cpp
 *
 * \date 2018-07-06
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cmath>
#include <cstddef>
#include <doctest.h>
#include <iostream>
#include <mutex>
#include <thread>

#include "../hdr/mariadb_modern_cpp.hpp"
#include "test_config.hpp"

TEST_CASE("concurrent") {

  SUBCASE("insert_id") {
    std::vector<std::thread> thds;
    std::mutex test_mutex;

    bool has_exception = false;

    for (int i = 0; i < 8; i++) {

      thds.emplace_back([&test_mutex, &has_exception, i]() {
        try {
          mariadb::database test_db(get_test_config());

          auto table_name = std::string("mariadb_modern_cpp_test.tmp_table") +
                            std::to_string(i);

          test_db << std::string(
              "CREATE TABLE IF NOT EXISTS " + table_name +
              " (id BIGINT PRIMARY KEY AUTO_INCREMENT NOT NULL);");
          test_db << std::string("INSERT INTO " + table_name + " VALUES ();");
          auto row_id = test_db.insert_id();
          test_db << std::string("INSERT INTO " + table_name + " VALUES ();");

          auto row_id2 = test_db.insert_id();

          {
            std::lock_guard lk(test_mutex);
            CHECK(row_id2 == row_id + 1);
          }
          test_db << std::string("drop TABLE " + table_name);
        } catch (const mariadb::mariadb_exception &e) {

          std::lock_guard lk(test_mutex);

          std::cerr << "catch exception: " << e.what() << std::endl;
          has_exception = true;
        }
      }

      );
    }
    for (auto &thd : thds) {
      thd.join();
    }

    CHECK(!has_exception);
  }
}
