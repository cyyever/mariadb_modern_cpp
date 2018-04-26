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
  config.user = "mariadb_modern_cpp_test";
  config.passwd = "123";
  sqlite::database test_db("mariadb_modern_cpp_test", config);

  SUBCASE("select without argument") {
    test_db << "select * from mariadb_modern_cpp_test.col_type_test;";
  }

  SUBCASE("select with arguments") {
    test_db << "select * from mariadb_modern_cpp_test.col_type_test where id=?;"
            << 0;
  }

  SUBCASE("extract without row") {
    bool has_exception = false;
    try {
      unsigned int val;
      test_db << "select uint_col from mariadb_modern_cpp_test.col_type_test "
                 "where id>1000;" >>
          val;
    } catch (const sqlite::errors::no_rows &) {
      has_exception = true;
    }
    CHECK(has_exception);
  }

  SUBCASE("extract BIGINT UNSIGNED") {
    unsigned int val;
    test_db << "select uint_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(val == 1);
  }
  SUBCASE("extract BIGINT") {
    int val;
    test_db << "select int_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(val == -1);
  }

  SUBCASE("extract DECIMAL UNSIGNED") {
    double val;
    test_db << "select udec_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(val == 0.3);
  }
  SUBCASE("extract DECIMAL") {
    double val;
    test_db << "select dec_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(val == -0.3);
  }

  SUBCASE("extract DOUBLE UNSIGNED") {
    double val;
    test_db << "select udouble_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(val == 0.3);
  }

  SUBCASE("extract DOUBLE") {
    double val;
    test_db << "select double_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(val == -0.3);
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
      int val;
      test_db << "select null_col from mariadb_modern_cpp_test.col_type_test "
	"where id=?;"
	<< 1 >>
	val;
    } catch (const sqlite::errors::can_not_hold_null &) {
      has_exception = true;
    }
    CHECK(has_exception);
  }

  SUBCASE("false optional type") {
    bool has_exception = false;
    try {
    std::optional<int> val;
      test_db << "select varchar_col from mariadb_modern_cpp_test.col_type_test "
	"where id=?;"
	<< 1 >>
	val;
    } catch (const sqlite::errors::unsupported_column_type&) {
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

  SUBCASE("extract NOT NULL") {
    std::optional<std::string> val;
    CHECK(!val.has_value());
 
    test_db << "update mariadb_modern_cpp_test.col_type_test set null_col= ? where id=?;"<<"not null"<<1;

    test_db << "select null_col from mariadb_modern_cpp_test.col_type_test "
               "where id=?;"
            << 1 >>
        val;
    CHECK(val.has_value());
    CHECK(val.value()=="not null");
    test_db << "update mariadb_modern_cpp_test.col_type_test set null_col= ? where id=?;"<<nullptr<<1;
  }
}
