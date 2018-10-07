#pragma once

#include <stdexcept>
#include <string>

#if __has_include(<mariadb/mysql.h>)
#include <mariadb/errmsg.h>
#include <mariadb/mysql.h>
#define USE_MARIADB
#elif __has_include(<mysql/mysql.h>)
#include <mysql/errmsg.h>
#include <mysql/mysql.h>
#define USE_MYSQL
#else
#error No mariadb/mysql header found!
#endif

namespace mariadb {
class mariadb_exception : public std::runtime_error {
public:
  mariadb_exception(std::string msg, std::string sql = "")
      : runtime_error(
            msg + (sql.empty() ? "" : (std::string(" \nerror sql :") + sql))),
        _sql(std::move(sql)) {}

  mariadb_exception(MYSQL *mysql, std::string sql = "")
      : mariadb_exception(mysql_error(mysql), std::move(sql)) {
    _errno = mysql_errno(mysql);
  }
  const std::string &get_sql() const noexcept { return _sql; }
  auto get_errno() const noexcept -> auto { return _errno; }

private:
  std::string _sql{};
  unsigned int _errno{CR_UNKNOWN_ERROR};
};

namespace exceptions {

// Some additional errors are here for the C++ interface
class connection : public mariadb_exception {
  using mariadb_exception::mariadb_exception;
};
class more_rows : public mariadb_exception {
  using mariadb_exception::mariadb_exception;
};
class no_rows : public mariadb_exception {
  using mariadb_exception::mariadb_exception;
};
class more_result_sets : public mariadb_exception {
  using mariadb_exception::mariadb_exception;
};
class no_result_sets : public mariadb_exception {
  using mariadb_exception::mariadb_exception;
};
class more_statements : public mariadb_exception {
  using mariadb_exception::mariadb_exception;
}; // Prepared statements can only contain one statement
class lack_prepare_arguments : public mariadb_exception {
  using mariadb_exception::mariadb_exception;
};
class more_prepare_arguments : public mariadb_exception {
  using mariadb_exception::mariadb_exception;
};
class out_of_row_range : public mariadb_exception {
  using mariadb_exception::mariadb_exception;
};
class unsupported_column_type : public mariadb_exception {
  using mariadb_exception::mariadb_exception;
};
class column_conversion : public mariadb_exception {
  using mariadb_exception::mariadb_exception;
};
class can_not_hold_null : public mariadb_exception {
  using mariadb_exception::mariadb_exception;
};
class bad_alignment : public mariadb_exception {
  using mariadb_exception::mariadb_exception;
};
} // namespace exceptions
} // namespace mariadb
