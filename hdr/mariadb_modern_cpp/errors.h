#pragma once

#include <string>
#include <stdexcept>

#include <sqlite3.h>
#include <mariadb/mysql.h>

namespace sqlite {

	class sqlite_exception: public std::runtime_error {
	public:
		sqlite_exception(const char* msg, std::string sql, int code = -1): runtime_error(msg), code(code), sql(sql) {
		
		puts(sql.c_str());}
		sqlite_exception(int code, std::string sql): runtime_error(sqlite3_errstr(code)), code(code), sql(sql) {
		
		puts(sql.c_str());}
		int get_code() const {return code & 0xFF;}
		int get_extended_code() const {return code;}
		std::string get_sql() const {return sql;}
	private:
		int code{0};
		std::string sql;
	};

	class mariadb_exception: public std::runtime_error {
	public:
		mariadb_exception(MYSQL* mysql, std::string sql): runtime_error(mysql_error(mysql)), sql(sql),code(mysql_errno(mysql)) {}
		std::string get_sql() const {return sql;}
		int get_code() const {return code;}
	private:
		std::string sql;
		int code;
	};

	namespace errors {

		//Some additional errors are here for the C++ interface
		class more_rows: public sqlite_exception { using sqlite_exception::sqlite_exception; };
		class no_rows: public sqlite_exception { using sqlite_exception::sqlite_exception; };
		class more_statements: public sqlite_exception { using sqlite_exception::sqlite_exception; }; // Prepared statements can only contain one statement
		class invalid_utf16: public sqlite_exception { using sqlite_exception::sqlite_exception; };
		class lack_prepare_arguments: public sqlite_exception { using sqlite_exception::sqlite_exception; };
		class more_prepare_arguments: public sqlite_exception { using sqlite_exception::sqlite_exception; };

		static void throw_mariadb_error(MYSQL* mysql, const std::string &sql = "") {
		  throw mariadb_exception(mysql, sql);
		}

	}
	namespace exceptions = errors;
}
