#pragma once

#include <stdexcept>
#include <string>

#include <mariadb/mysql.h>

namespace  mariadb{
class mariadb_exception : public std::runtime_error {
public:
  mariadb_exception(const char *msg, std::string sql = "")
      : runtime_error(msg), _sql(std::move(sql)) {}
  mariadb_exception(std::string msg, std::string sql = "")
      : runtime_error(std::move(msg)), _sql(std::move(sql)) {}
  mariadb_exception(MYSQL *mysql, std::string sql = "")
      : runtime_error(mysql_error(mysql)), _sql(std::move(sql)) {}
  const std::string &get_sql() const { return _sql; }

private:
  std::string _sql = "";
};

namespace exceptions {

// Some additional errors are here for the C++ interface
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
} // namespace exceptions
}
