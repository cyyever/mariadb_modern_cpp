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

TEST_CASE("select") {
  mariadb::mariadb_config config;
  config.host = "127.0.0.1";
  config.user = "mariadb_modern_cpp_test";
  config.passwd = "123";
  config.default_database = "mariadb_modern_cpp_test";
  mariadb::database test_db(config);

  SUBCASE("select without argument") {
    test_db << "select * from mariadb_modern_cpp_test.col_type_test;";
  }

  SUBCASE("select with arguments") {
    test_db << "select * from mariadb_modern_cpp_test.col_type_test where id=?;"
            << 0;
  }

  SUBCASE("select lacking argument") {
    bool has_exception = false;
    try {
      test_db
          << "select * from mariadb_modern_cpp_test.col_type_test where id=?;";
    } catch (const mariadb::exceptions::lack_prepare_arguments &) {
      has_exception = true;
    }
    CHECK(has_exception);
  }

  SUBCASE("select LONGBLOB") {
    std::vector<std::byte> val{};
    for (auto b : {'l', 'o', 'n', 'g', 'b', 'l', 'o', 'b'}) {
      val.push_back(static_cast<std::byte>(b));
    }
    size_t count = 0;
    test_db << "select count(*) from mariadb_modern_cpp_test.col_type_test "
               "where longblob_col=?;"
            << val >>
        count;

    CHECK(count == 1);
  }

  SUBCASE("extract with more than one row") {
    bool has_exception = false;
    try {
      std::string name;
      test_db << "CREATE TABLE IF NOT EXISTS mariadb_modern_cpp_test.tmp_table "
                 "(name TEXT);";
      test_db << "INSERT INTO tmp_table VALUES (?)"
              << "aa";
      test_db << "INSERT INTO tmp_table VALUES (?)"
              << "bb";
      test_db << "select name from mariadb_modern_cpp_test.tmp_table;" >> name;
    } catch (const mariadb::exceptions::more_rows &) {
      has_exception = true;
    }
    test_db << "drop TABLE mariadb_modern_cpp_test.tmp_table;";
    CHECK(has_exception);
  }

  SUBCASE("extract without row") {
    bool has_exception = false;
    try {
      uint64_t val{};
      test_db << "select uint_col from mariadb_modern_cpp_test.col_type_test "
                 "where id>1000;" >>
          val;
    } catch (const mariadb::exceptions::no_rows &) {
      has_exception = true;
    }
    CHECK(has_exception);
  }

  SUBCASE("extract BIGINT UNSIGNED") {
    uint64_t val{};
    test_db << "select uint_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(val == 1);
  }
  SUBCASE("extract BIGINT") {
    int64_t val{};
    test_db << "select int_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(val == -1);
  }

  SUBCASE("extract DECIMAL UNSIGNED") {
    long double val{};
    test_db << "select udec_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(std::fabs(val - 0.3) < 0.0000001);
  }
  SUBCASE("extract DECIMAL") {
    long double val{};
    test_db << "select dec_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(std::fabs(val + 0.3) < 0.0000001);
  }

  SUBCASE("extract DOUBLE UNSIGNED") {
    long double val{};
    test_db << "select udouble_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(std::fabs(val - 0.3) < 0.0000001);
  }

  SUBCASE("extract DOUBLE") {
    long double val{};
    test_db << "select double_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(std::fabs(val + 0.3) < 0.0000001);
  }
  SUBCASE("extract VARCHAR") {
    std::string val;
    test_db << "select varchar_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(val == "varchar");
  }
  SUBCASE("extract CHAR") {
    std::string val;
    test_db << "select char_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(val == "char");
  }
  SUBCASE("extract LONGTEXT") {
    std::string val;
    test_db << "select longtext_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(val == "longtext");
  }

  SUBCASE("extract LONGBLOB") {
    std::vector<std::byte> val;
    test_db << "select longblob_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;

    std::vector<std::byte> expected_val;
    for (auto b : {'l', 'o', 'n', 'g', 'b', 'l', 'o', 'b'}) {
      expected_val.push_back(static_cast<std::byte>(b));
    }
    CHECK(val == expected_val);
  }

  SUBCASE("can't hold NULL") {
    bool has_exception = false;
    try {
      int64_t val;
      test_db << "select null_col from mariadb_modern_cpp_test.col_type_test "
                 "where id=?;"
              << 1 >>
          val;
    } catch (const mariadb::exceptions::can_not_hold_null &) {
      has_exception = true;
    }
    CHECK(has_exception);
  }

  SUBCASE("false optional type") {
    bool has_exception = false;
    try {
      std::optional<int64_t> val;
      test_db << "select varchar_col from "
                 "mariadb_modern_cpp_test.col_type_test "
                 "where id=?;"
              << 1 >>
          val;
    } catch (const mariadb::exceptions::unsupported_column_type &) {
      has_exception = true;
    }
    CHECK(has_exception);
  }

  SUBCASE("extract NULL") {
    std::optional<std::string> val;

    test_db << "select null_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(!val.has_value());
  }

  SUBCASE("extract optional and update NOT NULL") {
    std::optional<std::string> val;

    test_db << "update mariadb_modern_cpp_test.col_type_test set null_col= ? "
               "where id=?;"
            << "not null" << 1;

    test_db << "select null_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(val.has_value());
    CHECK(val.value() == "not null");
    val.reset();
    test_db << "update mariadb_modern_cpp_test.col_type_test set null_col= ? "
               "where id=?;"
            << val << 1;

    test_db << "select null_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(!val.has_value());
  }

  SUBCASE("extract unique_ptr and update NOT NULL") {
    std::unique_ptr<std::string> val;

    test_db << "update mariadb_modern_cpp_test.col_type_test set null_col= ? "
               "where id=?;"
            << "not null" << 1;

    test_db << "select null_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(val);
    CHECK(*val == "not null");
    val.reset();
    test_db << "update mariadb_modern_cpp_test.col_type_test set null_col= ? "
               "where id=?;"
            << val << 1;

    test_db << "select null_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(!val);
  }

  SUBCASE("extract LONGTEXT and NULL by std::tie") {
    std::string val;
    std::optional<std::string> val2;
    test_db << "select longtext_col,null_col from "
               "mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        std::tie(val, val2);
    CHECK(val == "longtext");
    CHECK(!val2.has_value());
  }

  SUBCASE("std::tie with less column selected") {
    bool has_exception = false;
    try {
      std::string val;
      std::optional<std::string> val2;
      test_db << "select longtext_col from "
                 "mariadb_modern_cpp_test.col_type_test "
                 "where id=?;"
              << 1 >>
          std::tie(val, val2);
    } catch (const mariadb::exceptions::out_of_row_range &) {
      has_exception = true;
    }
    CHECK(has_exception);
  }

  SUBCASE("extract LONGTEXT and NULL by callback") {
    test_db << "select longtext_col,null_col from "
               "mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>

        [](std::string val, std::optional<std::string> val2) {
          CHECK(val == "longtext");
          CHECK(!val2.has_value());
        };
  }

  SUBCASE("select and extract LONGBLOB by std::vector<double>") {
    std::vector<double> val{1.0, 2.0, 0.0};
    size_t count = 0;

    test_db << "CREATE TABLE IF NOT EXISTS mariadb_modern_cpp_test.tmp_table "
               "(digits LONGBLOB);";

    test_db << "INSERT INTO tmp_table VALUES (?)" << val;

    test_db << "select count(*) from mariadb_modern_cpp_test.tmp_table where "
               "digits =?;"
            << val >>
        count;

    test_db << "drop TABLE mariadb_modern_cpp_test.tmp_table;";
    CHECK(count == 1);
  }

  SUBCASE("used and reexecutes sql") {
    auto ps = test_db
              << "select count(*) from mariadb_modern_cpp_test.col_type_test;";
    ps.used(true);
    size_t count = 0;
    ps.execute();
    ps >> count;
    CHECK(count == 1);
  }

  SUBCASE("disable multistatement") {
    bool has_exception = false;
    try {
      test_db << "select count(*) from "
                 "mariadb_modern_cpp_test.col_type_test;select count(*) from "
                 "mariadb_modern_cpp_test.col_type_test;";
    } catch (const mariadb::mariadb_exception &e) {
      has_exception = true;
    }
    CHECK(has_exception);
  }
}
