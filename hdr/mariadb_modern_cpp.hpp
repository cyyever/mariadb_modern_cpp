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
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>

#include "mariadb_modern_cpp/errors.hpp"
#include "mariadb_modern_cpp/utility/function_traits.hpp"

namespace mariadb {

struct mariadb_config {
  std::optional<std::string> host;
  std::optional<unsigned int> port;
  std::optional<std::string> unix_socket;
  std::string user;
  std::string passwd;
  std::optional<std::string> default_database;
  std::chrono::seconds connect_timeout{10};
  std::chrono::seconds read_timeout{120};
  std::chrono::seconds write_timeout{10};
};

template <typename Test, template <typename...> class Ref>
struct is_specialization_of : std::false_type {};

template <template <typename...> class Ref, typename... Args>
struct is_specialization_of<Ref<Args...>, Ref> : std::true_type {};

template <typename Type>
struct is_mariadb_value
    : public std::integral_constant<
          bool, std::is_floating_point<Type>::value ||
                    std::is_integral<Type>::value ||
                    std::is_same<std::string, Type>::value> {};

template <typename Type, typename Allocator>
struct is_mariadb_value<std::vector<Type, Allocator>>
    : public std::integral_constant<bool,
                                    std::is_floating_point<Type>::value ||
                                        std::is_integral<Type>::value ||
                                        std::is_same<std::byte, Type>::value> {
};

template <typename OptionalT>
struct is_mariadb_value<std::optional<OptionalT>>
    : public std::integral_constant<bool, is_mariadb_value<OptionalT>::value> {
};

template <typename T>
struct is_mariadb_value<std::unique_ptr<T>>
    : public std::integral_constant<bool, is_mariadb_value<T>::value> {};

class statement_binder {

public:
  // statement_binder is not copyable
  statement_binder() = delete;
  statement_binder(const statement_binder &other) = delete;
  statement_binder &operator=(const statement_binder &) = delete;

  void execute() {
    used(true);

    if (!_unprepared_sql_part.empty()) {
      throw exceptions::lack_prepare_arguments(
          "lacks some arguments to prepare sql",
          std::string(_unprepared_sql_part.data(),
                      _unprepared_sql_part.size()));
    }

    if (mysql_real_query(_db.get(), _full_sql.c_str(), _full_sql.size()) != 0) {
      throw mariadb_exception(_db.get(), _full_sql);
    }
    _reset();
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
  bool used() const noexcept { return execution_started; }

private:
  std::shared_ptr<MYSQL> _db;
  std::string _sql;
  std::string_view _unprepared_sql_part;
  std::string _full_sql;
  MYSQL_ROW row{};
  unsigned long *lengths{};
  MYSQL_FIELD *fields{};
  unsigned int field_count{};

  bool execution_started = false;

  void _reset() {
    _unprepared_sql_part = _sql;
    _full_sql.clear();
    _consume_prepared_sql_part();
  }

  void _consume_prepared_sql_part() {
    const auto pos = _unprepared_sql_part.find('?');
    if (pos == _unprepared_sql_part.npos) {
      _full_sql.append(_unprepared_sql_part.data(),
                       _unprepared_sql_part.size());
      _unprepared_sql_part.remove_prefix(_unprepared_sql_part.size());
    } else if (pos != 0) {
      _full_sql.append(_unprepared_sql_part.data(), pos);
      _unprepared_sql_part.remove_prefix(pos);
    }
  }

  void _extract(std::function<void(void)> call_back) {
    if (!used()) {
      execute();
    }

    auto result_set = std::shared_ptr<MYSQL_RES>(
        mysql_store_result(_db.get()), [this](MYSQL_RES * ptr) noexcept {
          row = {};
          fields = {};
          field_count = {};
          mysql_free_result(ptr);
        });

    if (!result_set) {
      throw exceptions::no_result_sets(
          "no result sets to extract: exactly 1 result set expected", sql());
    }

    const auto row_num = mysql_num_rows(result_set.get());
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

    auto result_set = std::shared_ptr<MYSQL_RES>(
        mysql_store_result(_db.get()), [this](MYSQL_RES * ptr) noexcept {
          row = {};
          fields = {};
          field_count = {};
          mysql_free_result(ptr);
        });

    if (!result_set) {
      throw exceptions::no_result_sets(
          "no result sets to extract: exactly 1 result set expected", sql());
    }

    const auto row_num = mysql_num_rows(result_set.get());
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

  friend statement_binder &append_string_argument(statement_binder &db,
                                                  const char *str, size_t size);

  template <typename Result>
  typename std::enable_if<is_mariadb_value<Result>::value, void>::type
  _get_col_from_row(unsigned int idx, Result &val) {

    if (idx >= field_count) {
      throw exceptions::out_of_row_range(
          std::string("try to access column ") + std::to_string(idx) +
              " ,exceeds column count " + std::to_string(field_count),
          sql());
    }

    const auto field_type = fields[idx].type;

    if constexpr (is_specialization_of<Result, std::optional>::value) {
      if (!row[idx]) {
        val.reset();
        return;
      } else {
        typename Result::value_type real_value;
        _get_col_from_row(idx, real_value);
        val = std::move(real_value);
        return;
      }
    } else if constexpr (is_specialization_of<Result, std::unique_ptr>::value) {
      if (!row[idx]) {
        val.reset();
        return;
      } else {
        typename Result::element_type real_value;
        _get_col_from_row(idx, real_value);
        val = std::make_unique<typename Result::element_type>(
            std::move(real_value));
        return;
      }
    } else {
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
        if (fields[idx].flags & UNSIGNED_FLAG) {
          val = static_cast<Result>(::strtoull(row[idx], nullptr, 10));
        } else {
          val = static_cast<Result>(::strtoll(row[idx], nullptr, 10));
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
      } else if constexpr (is_specialization_of<Result, std::vector>::value) {
        if (lengths[idx] % sizeof(typename Result::value_type) != 0) {
          throw exceptions::bad_alignment(
              std::string("column ") + std::to_string(idx) + " type " +
                  std::to_string(field_type) + " can't be stored in argument",
              sql());
        }
        val.resize(lengths[idx] / sizeof(typename Result::value_type));
        memcpy(val.data(), row[idx], lengths[idx]);
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
  statement_binder(std::shared_ptr<MYSQL> db, std::string sql)
      : _db(db), _sql(std::move(sql)), _unprepared_sql_part(_sql) {
    _reset();
  }

  ~statement_binder() noexcept(false) {
    if (!used() && std::uncaught_exceptions() == 0) {
      execute();
      used(true);
    }
  }

  template <typename Result>
  typename std::enable_if<is_mariadb_value<Result>::value, void>::type
  operator>>(Result &value) {
    this->_extract_single_value(
        [&value, this] { _get_col_from_row(0, value); });
  }

  template <typename Tuple, int Element = 0,
            bool Last = (std::tuple_size<Tuple>::value == Element)>
  struct tuple_iterate {
    static void iterate(Tuple &t, statement_binder &db) {
      db._get_col_from_row(Element, std::get<Element>(t));
      tuple_iterate<Tuple, Element + 1>::iterate(t, db);
    }
  };

  template <typename Tuple, int Element>
  struct tuple_iterate<Tuple, Element, true> {
    static void iterate(Tuple &, statement_binder &) {}
  };

  template <typename... Types> void operator>>(std::tuple<Types...> &&values) {
    this->_extract_single_value([&values, this]() {
      tuple_iterate<std::tuple<Types...>>::iterate(values, *this);
    });
  }

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

    template <typename Function, typename... Values,
              std::size_t Boundary = Count>
    static typename std::enable_if<(sizeof...(Values) < Boundary), void>::type
    run(statement_binder &db, Function &&function, Values &&... values) {
      typename std::remove_cv<typename std::remove_reference<
          nth_argument_type<Function, sizeof...(Values)>>::type>::type value{};
      db._get_col_from_row(sizeof...(Values), value);

      run<Function>(db, function, std::forward<Values>(values)...,
                    std::move(value));
    }

    template <typename Function, typename... Values,
              std::size_t Boundary = Count>
    static typename std::enable_if<(sizeof...(Values) == Boundary), void>::type
    run(statement_binder &, Function &&function, Values &&... values) {
      function(std::move(values)...);
    }
  };

  template <typename Function>
  typename std::enable_if<!is_mariadb_value<Function>::value, void>::type
  operator>>(Function &&func) {
    typedef utility::function_traits<Function> traits;

    this->_extract(
        [&func, this]() { binder<traits::arity>::run(*this, func); });
  }

  // Convert char* to string to trigger op<<(..., const std::string )
  template <std::size_t N>
  inline statement_binder &operator<<(const char (&STR)[N]) {
    return append_string_argument(STR, N - 1);
  }

  template <typename Argument>

  typename std::enable_if<
      is_mariadb_value<typename std::remove_cv<
          typename std::remove_reference<Argument>::type>::type>::value,
      statement_binder &>::type

  operator<<(Argument &&val) {
    using raw_argument_type = typename std::remove_cv<
        typename std::remove_reference<Argument>::type>::type;

    if constexpr (std::is_same_v<raw_argument_type, std::string>) {
      return append_string_argument(val.c_str(), val.size());
    } else if constexpr (is_specialization_of<raw_argument_type,
                                              std::vector>::value) {
      return append_string_argument(
          val.data(),
          val.size() * sizeof(typename raw_argument_type::value_type));
    } else {

      if (_unprepared_sql_part.empty()) {
        throw exceptions::more_prepare_arguments(
            "no extra arguments needed to prepare sql", _sql);
      }

      if constexpr (is_specialization_of<raw_argument_type,
                                         std::optional>::value) {
        if (val.has_value()) {
          return (*this) << (*val);
        } else {
          _full_sql.append("NULL");
        }
      } else if constexpr (is_specialization_of<raw_argument_type,
                                                std::unique_ptr>::value) {
        if (val.get()) {
          return (*this) << (*val);
        } else {
          _full_sql.append("NULL");
        }
      } else {
        _full_sql.append(std::to_string(std::forward<Argument>(val)));
      }

      _unprepared_sql_part.remove_prefix(1);
      _consume_prepared_sql_part();
      return (*this);
    }
  }

  statement_binder &append_string_argument(const void *str, size_t size) {

    if (_unprepared_sql_part.empty()) {
      throw exceptions::more_prepare_arguments(
          "no extra arguments needed to prepare sql", _sql);
    }

    std::string escaped_str(size * 2 + 1, '\0');
    auto const real_size = mysql_real_escape_string(
        _db.get(), escaped_str.data(), static_cast<const char *>(str), size);
    escaped_str.resize(real_size);

    if (real_size == static_cast<unsigned long>(-1)) {
      throw mariadb_exception(_db.get());
    }

    _full_sql.push_back('\'');
    _full_sql.append(escaped_str.data(), escaped_str.size());
    _full_sql.push_back('\'');
    _unprepared_sql_part.remove_prefix(1);
    _consume_prepared_sql_part();
    return (*this);
  }
};

class transaction_context final {
public:
  // transaction_context is not copyable
  transaction_context() = delete;
  transaction_context(const transaction_context &other) = delete;
  transaction_context &operator=(const transaction_context &) = delete;

  transaction_context(std::shared_ptr<MYSQL> db)
      : _db(db), _transaction_statment(db, "begin;") {
    _transaction_statment.execute();
  }

  ~transaction_context() noexcept {
    if (std::uncaught_exceptions()) {
      _db.reset();
      return;
    }
    try {
      _transaction_statment.execute();
      statement_binder(_db, "commit;").execute();
    } catch (...) {
      _db.reset();
    }
  }

  statement_binder &operator<<(const std::string &sql) {
    return _transaction_statment << sql;
  }

private:
  std::shared_ptr<MYSQL> _db;
  statement_binder _transaction_statment;
};

struct library_initer {
  library_initer() noexcept { mysql_library_init(0, nullptr, nullptr); }
  ~library_initer() noexcept { mysql_library_end(); }
};
inline void init_library() noexcept { static library_initer initer; }

struct thread_setting {
  thread_setting() noexcept { mysql_thread_init(); }
  ~thread_setting() noexcept { mysql_thread_end(); }
};
inline void init_thread() noexcept { thread_local thread_setting setting; }

class database {
protected:
  std::shared_ptr<MYSQL> _db;

public:
  // database is not copyable
  database() = delete;
  database(const database &other) = delete;
  database &operator=(const database &) = delete;

  database(const mariadb_config &config) : _db(nullptr) {
    init_library();
    init_thread();
    MYSQL *tmp = mysql_init(nullptr);
    if (!tmp) {
      throw mariadb_exception("mysql_init failed");
    }
    _db = std::shared_ptr<MYSQL>(tmp, [=](MYSQL * ptr) noexcept {
      mysql_close(ptr);
    }); // this will close the connection eventually when no longer needed.

    unsigned int seconds_count =
        static_cast<unsigned int>(config.connect_timeout.count());
    auto res = mysql_options(tmp, MYSQL_OPT_CONNECT_TIMEOUT, &seconds_count);
    if (res != 0)
      throw mariadb_exception("MYSQL_OPT_CONNECT_TIMEOUT failed");

    seconds_count = static_cast<unsigned int>(config.read_timeout.count());
    res = mysql_options(tmp, MYSQL_OPT_READ_TIMEOUT, &seconds_count);
    if (res != 0)
      throw mariadb_exception("MYSQL_OPT_READ_TIMEOUT failed");

    seconds_count = static_cast<unsigned int>(config.write_timeout.count());
    res = mysql_options(tmp, MYSQL_OPT_WRITE_TIMEOUT, &seconds_count);
    if (res != 0)
      throw mariadb_exception("MYSQL_OPT_WRITE_TIMEOUT failed");

    {
      // mysql_real_connect is not thread safe,so we use mutex to protect
      static std::mutex connect_mtx;
      std::lock_guard lk(connect_mtx);
      if (!mysql_real_connect(
              tmp, config.host ? config.host.value().c_str() : nullptr,
              config.user.c_str(), config.passwd.c_str(),
              config.default_database ? config.default_database.value().c_str()
                                      : nullptr,
              config.port ? config.port.value() : 0,
              config.unix_socket ? config.unix_socket.value().c_str() : nullptr,
              CLIENT_FOUND_ROWS)) {
        throw exceptions::connection(_db.get());
      }
    }
  }

  statement_binder operator<<(const std::string &sql) {
    return statement_binder(_db, sql);
  }

  transaction_context get_transaction_context() {
    return transaction_context(_db);
  }

  auto connection() const noexcept -> auto { return _db; }

  my_ulonglong insert_id() const noexcept { return mysql_insert_id(_db.get()); }
}; // namespace mariadb

} // namespace mariadb
