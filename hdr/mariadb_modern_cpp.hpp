#pragma once

#if __has_include(<mariadb/mysql.h>)
#include <mariadb/mysql.h>
#define USE_MARIADB
#elif __has_include(<mysql/mysql.h>)
#include <mysql/mysql.h>
#define USE_MYSQL
#else
#error No mariadb/mysql header found!
#endif

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>

#include "mariadb_modern_cpp/errors.h"
#include "mariadb_modern_cpp/utility/function_traits.h"

namespace sqlite {

class database;
class database_binder;

template <std::size_t> class binder;

class database_binder {

public:
  // database_binder is not copyable
  database_binder() = delete;
  database_binder(const database_binder &other) = delete;
  database_binder &operator=(const database_binder &) = delete;

  void execute() {
    used(true);

    if (!_unprepared_sql_part.empty()) {
      throw exceptions::lack_prepare_arguments(
          "lacks some arguments to prepare sql",
          std::string(_unprepared_sql_part.data(),
                      _unprepared_sql_part.size()));
    }

    auto full_sql = _sql_stream.str();

    if (mysql_real_query(_db.get(), full_sql.c_str(), full_sql.size()) != 0) {
      throw mariadb_exception(_db.get(), full_sql);
    }
  }

  std::string sql() { return _sql; }

  void used(bool state) {
    if (state) {
      while (mysql_more_results(_db.get())) {
        auto reset_set = mysql_use_result(_db.get());
        if (!reset_set) {
          throw mariadb_exception(_db.get());
        }
        mysql_free_result(reset_set);
      }
    }
    execution_started = state;
  }
  bool used() const { return execution_started; }

private:
  std::shared_ptr<MYSQL> _db;
  std::string _sql;
  std::string_view _unprepared_sql_part;
  std::ostringstream _sql_stream;
  MYSQL_ROW row{};
  unsigned long *lengths{};
  MYSQL_FIELD *fields{};
  unsigned int field_count{};

  bool execution_started = false;

  void _consume_prepared_sql_part() {
    auto pos = _unprepared_sql_part.find('?');
    if (pos == _unprepared_sql_part.npos) {
      _sql_stream.write(_unprepared_sql_part.data(),
                        _unprepared_sql_part.size());
      _unprepared_sql_part.remove_prefix(_unprepared_sql_part.size());
    } else if (pos != 0) {
      _sql_stream.write(_unprepared_sql_part.data(), pos);
      _unprepared_sql_part.remove_prefix(pos);
    }
  }

  void _extract(std::function<void(void)> call_back) {
    if (!used()) {
      execute();
    }

    auto result_set = std::shared_ptr<MYSQL_RES>(mysql_store_result(_db.get()),
                                                 [this](MYSQL_RES *ptr) {
                                                   row = {};
                                                   fields = {};
                                                   field_count = {};
                                                   mysql_free_result(ptr);
                                                 });

    if (!result_set) {
      throw exceptions::no_result_sets(
          "no result sets to extract: exactly 1 result set expected", sql());
    }

    auto row_num = mysql_num_rows(result_set.get());
    for (size_t i = 0; i < row_num; i++) {
      row = mysql_fetch_row(result_set.get());
      lengths = mysql_fetch_lengths(result_set.get());
      fields = mysql_fetch_fields(result_set.get());
      field_count = mysql_field_count(_db.get());
      call_back();
    }

    if (mysql_more_results(_db.get())) {
      throw exceptions::more_result_sets("no all result sets extracted", sql());
    }
  }

  void _extract_single_value(std::function<void(void)> call_back) {
    if (!used()) {
      execute();
    }

    auto result_set = std::shared_ptr<MYSQL_RES>(mysql_store_result(_db.get()),
                                                 [this](MYSQL_RES *ptr) {
                                                   row = {};
                                                   fields = {};
                                                   field_count = {};
                                                   mysql_free_result(ptr);
                                                 });

    if (!result_set) {
      throw exceptions::no_result_sets(
          "no result sets to extract: exactly 1 result set expected", sql());
    }

    auto row_num = mysql_num_rows(result_set.get());
    if (row_num == 0) {
      throw exceptions::no_rows("no rows to extract: exactly 1 row expected",
                                sql());
    } else if (row_num > 1) {
      throw exceptions::more_rows("not all rows extracted", sql());
    }

    row = mysql_fetch_row(result_set.get());
    lengths = mysql_fetch_lengths(result_set.get());
    fields = mysql_fetch_fields(result_set.get());
    field_count = mysql_field_count(_db.get());
    call_back();

    if (mysql_more_results(_db.get())) {
      throw exceptions::more_result_sets("no all result sets extracted", sql());
    }
  }

  template <typename Type>
  struct is_mariadb_value
      : public std::integral_constant<
            bool, std::is_floating_point<Type>::value ||
                      std::is_integral<Type>::value ||
                      std::is_same<std::string, Type>::value ||
                      std::is_same<std::u16string, Type>::value ||
                      std::is_same<std::vector<std::byte>, Type>::value> {};

  template <typename OptionalT>
  struct is_mariadb_value<std::optional<OptionalT>>
      : public std::integral_constant<bool,
                                      is_mariadb_value<OptionalT>::value> {};

  template <typename Test, template <typename...> class Ref>
  struct is_specialization_of : std::false_type {};

  template <template <typename...> class Ref, typename... Args>
  struct is_specialization_of<Ref<Args...>, Ref> : std::true_type {};

  /* for nullptr & unique_ptr support */
  friend database_binder &operator<<(database_binder &db, std::nullptr_t);
  template <typename T> friend T operator++(database_binder &db, int);
  // Overload instead of specializing function templates
  // (http://www.gotw.ca/publications/mill17.htm)
  friend database_binder &append_string_argument(database_binder &db,
                                                 const char *str, size_t size);

  template <typename Argument>
  friend database_binder &operator<<(database_binder &db, Argument &&val);

  template <typename OptionalT>
  friend database_binder &operator<<(database_binder &db,
                                     const std::optional<OptionalT> &val);

  template <typename Result>
  typename std::enable_if<is_mariadb_value<Result>::value, void>::type
  get_col_from_row(unsigned int idx, Result &val, bool check_null = true) {

    if (idx >= field_count) {
      throw exceptions::out_of_row_range(
          std::string("try to access column ") + std::to_string(idx) +
              " ,exceeds column count " + std::to_string(field_count),
          sql());
    }

    auto field_type = fields[idx].type;
    auto field_flags = fields[idx].flags;

    constexpr bool is_optional =
        is_specialization_of<Result, std::optional>::value;

    if constexpr (is_optional) {
      if (!row[idx]) {
        return;
      } else {
        typename Result::value_type real_value;
        get_col_from_row(idx, real_value, false);
        val = std::move(real_value);
        return;
      }
    }

    if (check_null) {
      if (!row[idx]) {
        throw exceptions::can_not_hold_null(
            std::string("column ") + std::to_string(idx) +
                " can be NULL,can't be stored in "
                "argument type,try std::optional",
            sql());
      }
    }

    switch (field_type) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_INT24:
      if constexpr (std::is_integral_v<Result>) {

        errno = 0;
        if (field_flags & UNSIGNED_FLAG) {
          val = ::strtoull(row[idx], nullptr, 10);
        } else {
          val = ::strtoll(row[idx], nullptr, 10);
        }
        if (errno != 0) {
          throw exceptions::column_conversion(
              std::string("converting column ") + std::to_string(idx) +
                  " to integer failed",
              sql());
        }
        return;
      } else {
        break;
      }
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
      if constexpr (std::is_floating_point_v<Result>) {
        errno = 0;
        val = ::strtold(row[idx], nullptr);
        if (errno != 0) {
          throw exceptions::column_conversion(
              std::string("converting column ") + std::to_string(idx) +
                  " to floating point failed",
              sql());
        }
        return;
      } else {
        break;
      }
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING:
      if constexpr (std::is_same_v<Result, std::string>) {
        val = std::string(row[idx], lengths[idx]);
        return;
      } else {
        break;
      }

    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
      if constexpr (std::is_same_v<Result, std::string>) {
        /*
       To distinguish between binary and nonbinary data for string data types,
       check whether the charsetnr value is 63. If so, the character set is
       binary, which indicates binary rather than nonbinary data. This enables
       you to distinguish BINARY from CHAR, VARBINARY from VARCHAR, and the BLOB
       types from the TEXT types.

       see https://dev.mysql.com/doc/refman/8.0/en/c-api-data-structures.html
       */

        if (fields[idx].charsetnr != 63) {
          val = std::string(row[idx], lengths[idx]);
          return;
        }
      } else if constexpr (std::is_same_v<Result, std::vector<std::byte>>) {
        auto byte_ptr = reinterpret_cast<std::byte *>(row[idx]);
        val = std::vector<std::byte>(byte_ptr, byte_ptr + lengths[idx]);
        return;
      }
      break;
    default:
      break;
    }

    throw exceptions::unsupported_column_type(
        std::string("column ") + std::to_string(idx) + " type " +
            std::to_string(field_type) + " is not supported",
        sql());
  }

public:
  database_binder(std::shared_ptr<MYSQL> db, std::string sql)
      : _db(db), _sql(std::move(sql)), _unprepared_sql_part(_sql) {
    _consume_prepared_sql_part();
  }

  ~database_binder() noexcept(false) {
    if (!used() && std::uncaught_exceptions() == 0) {
      execute();
      used(true);
    }
  }

  template <typename Result>
  typename std::enable_if<is_mariadb_value<Result>::value, void>::type
  operator>>(Result &value) {
    this->_extract_single_value([&value, this] { get_col_from_row(0, value); });
  }

  template <typename Tuple, int Element = 0,
            bool Last = (std::tuple_size<Tuple>::value == Element)>
  struct tuple_iterate {
    static void iterate(Tuple &t, database_binder &db) {
      db.get_col_from_row(Element, std::get<Element>(t));
      tuple_iterate<Tuple, Element + 1>::iterate(t, db);
    }
  };

  template <typename Tuple, int Element>
  struct tuple_iterate<Tuple, Element, true> {
    static void iterate(Tuple &, database_binder &) {}
  };

  template <typename... Types> void operator>>(std::tuple<Types...> &&values) {
    this->_extract_single_value([&values, this]() {
      tuple_iterate<std::tuple<Types...>>::iterate(values, *this);
    });
  }

  template <typename Function>
  typename std::enable_if<!is_mariadb_value<Function>::value, void>::type
  operator>>(Function &&func) {
    typedef utility::function_traits<Function> traits;

    this->_extract(
        [&func, this]() { binder<traits::arity>::run(*this, func); });
  }
};

struct mariadb_config {
  std::string host{"localhost"};
  unsigned int port{3306};
  std::string user;
  std::string passwd;
  std::string unix_socket;
  unsigned long connect_flags{CLIENT_FOUND_ROWS};
};

class database {
protected:
  std::shared_ptr<MYSQL> _db;

public:
  database(const std::string &db_name, const mariadb_config &config = {})
      : _db(nullptr) {
    MYSQL *tmp = mysql_init(nullptr);
    if (!tmp) {
      throw std::runtime_error("mysql_init failed");
    }

    auto ret = mysql_real_connect(
        tmp, config.host.c_str(), config.user.c_str(), config.passwd.c_str(),
        db_name.c_str(), config.port,
        config.unix_socket.empty() ? nullptr : config.unix_socket.c_str(),
        config.connect_flags);
    _db = std::shared_ptr<MYSQL>(tmp, [=](MYSQL *ptr) {
      mysql_close(ptr);
    }); // this will close the connection eventually when no longer needed.
    if (!ret)
      throw mariadb_exception(_db.get());
  }

  database(std::shared_ptr<MYSQL> db) : _db(db) {}

  database_binder operator<<(const std::string &sql) {
    return database_binder(_db, sql);
  }

  database_binder operator<<(const char *sql) {
    return *this << std::string(sql);
  }

  auto connection() const -> auto { return _db; }

  my_ulonglong insert_id() const { return mysql_insert_id(_db.get()); }
};

template <std::size_t Count> class binder {
private:
  template <typename Function, std::size_t Index>
  using nth_argument_type =
      typename utility::function_traits<Function>::template argument<Index>;

public:
  // `Boundary` needs to be defaulted to `Count` so that the `run` function
  // template is not implicitly instantiated on class template instantiation.
  // Look up section 14.7.1 _Implicit instantiation_ of the ISO C++14 Standard
  // and the
  // [dicussion](https://github.com/aminroosta/sqlite_modern_cpp/issues/8) on
  // Github.

  template <typename Function, typename... Values, std::size_t Boundary = Count>
  static typename std::enable_if<(sizeof...(Values) < Boundary), void>::type
  run(database_binder &db, Function &&function, Values &&... values) {
    typename std::remove_cv<typename std::remove_reference<
        nth_argument_type<Function, sizeof...(Values)>>::type>::type value{};
    // get_col_from_db(db, sizeof...(Values), value);

    run<Function>(db, function, std::forward<Values>(values)...,
                  std::move(value));
  }

  template <typename Function, typename... Values, std::size_t Boundary = Count>
  static typename std::enable_if<(sizeof...(Values) == Boundary), void>::type
  run(database_binder &, Function &&function, Values &&... values) {
    function(std::move(values)...);
  }
};

template <typename Argument>
inline database_binder &operator<<(database_binder &db, Argument &&val) {

  if constexpr (std::is_same_v<typename std::remove_reference<Argument>::type,
                               std::string> ||
                std::is_same_v<typename std::remove_reference<Argument>::type,
                               std::u16string> ||
                std::is_same_v<typename std::remove_reference<Argument>::type,
                               std::u32string>) {
    return append_string_argument(db, val.c_str(), val.size());
  } else {

    if (db._unprepared_sql_part.empty()) {
      throw exceptions::more_prepare_arguments(
          "no extra arguments needed to prepare sql", db._sql);
    }
    db._sql_stream << std::forward<Argument>(val);
    db._unprepared_sql_part.remove_prefix(1);
    db._consume_prepared_sql_part();
    return db;
  }
}
/* for nullptr support */
inline database_binder &operator<<(database_binder &db, std::nullptr_t) {
  if (db._unprepared_sql_part.empty()) {
    throw exceptions::more_prepare_arguments(
        "no extra arguments needed to prepare sql", db._sql);
  }
  db._sql_stream << "NULL";
  db._unprepared_sql_part.remove_prefix(1);
  db._consume_prepared_sql_part();
  return db;
}

// Convert char* to string to trigger op<<(..., const std::string )
template <std::size_t N>
inline database_binder &operator<<(database_binder &db, const char (&STR)[N]) {
  return append_string_argument(db, STR, N - 1);
}

inline database_binder &append_string_argument(database_binder &db,
                                               const char *str, size_t size) {

  if (db._unprepared_sql_part.empty()) {
    throw exceptions::more_prepare_arguments(
        "no extra arguments needed to prepare sql", db._sql);
  }

  std::string escaped_str(size * 2 + 1, '\0');
  auto real_size =
      mysql_real_escape_string(db._db.get(), escaped_str.data(), str, size);
  escaped_str.resize(real_size);

  db._sql_stream << '"';
  db._sql_stream.write(escaped_str.data(), escaped_str.size());
  db._sql_stream << '"';
  db._unprepared_sql_part.remove_prefix(1);
  db._consume_prepared_sql_part();
  return db;
}

// std::optional support for NULL values
template <typename OptionalT>
inline database_binder &operator<<(database_binder &db,
                                   const std::optional<OptionalT> &val) {
  if (val) {
    return db << std::move(*val);
  } else {
    return db << nullptr;
  }
}

// Convert the rValue binder to a reference and call first op<<, its needed for
// the call that creates the binder (be carefull of recursion here!)
template <typename T>
database_binder &&operator<<(database_binder &&db, const T &val) {
  db << val;
  return std::move(db);
}

} // namespace sqlite
