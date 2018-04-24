#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <functional>
#include <ctime>
#include <tuple>
#include <memory>
#include <sstream>
#include <optional>

#include <sqlite3.h>
#include <mariadb/mysql.h>

#include "mariadb_modern_cpp/errors.h"
#include "mariadb_modern_cpp/utility/function_traits.h"
#include "mariadb_modern_cpp/utility/uncaught_exceptions.h"


namespace sqlite {

        // std::optional support for NULL values
	template<class T>
	using optional = std::optional<T>;

	class database;
	class database_binder;

	template<std::size_t> class binder;

	template<typename Tuple, int Element = 0, bool Last = (std::tuple_size<Tuple>::value == Element)> struct tuple_iterate {
		static void iterate(Tuple& t, database_binder& db) {
			get_col_from_db(db, Element, std::get<Element>(t));
			tuple_iterate<Tuple, Element + 1>::iterate(t, db);
		}
	};

	template<typename Tuple, int Element> struct tuple_iterate<Tuple, Element, true> {
		static void iterate(Tuple&, database_binder&) {}
	};

	class database_binder {

	public:
		// database_binder is not copyable
		database_binder() = delete;
		database_binder(const database_binder& other) = delete;
		database_binder& operator=(const database_binder&) = delete;

		void execute() {
		  _start_execute();

		  if(!_unprepared_sql_part.empty()) {
		    throw errors::lack_prepare_arguments("lacks some arguments to prepare sql",std::string(_unprepared_sql_part.data(),_unprepared_sql_part.size()));
		  }

		  auto full_sql=_sql_stream.str();

		  if(mysql_real_query(_db.get(),full_sql.c_str(),full_sql.size())!=0) {
		    errors::throw_mariadb_error(_db.get(),full_sql);
		  }

		  auto reset_set= mysql_use_result(_db.get());
		  if(!reset_set) {
		    errors::throw_mariadb_error(_db.get());
		  }
		  mysql_free_result(reset_set);
		}
		
		std::string sql() {
		  return _sql;
		}

		void used(bool state) {
		  execution_started = state; 
		}
		bool used() const { return execution_started; }

	private:
		std::shared_ptr<MYSQL> _db;
		std::string _sql;
		std::string_view _unprepared_sql_part;
		std::ostringstream _sql_stream;
		utility::UncaughtExceptionDetector _has_uncaught_exception;


		bool execution_started = false;

		void _start_execute() {
		  used(true);
		}

		void _extract(std::function<void(void)> call_back) {
		  /*
			int hresult;
			_start_execute();

			while((hresult = sqlite3_step(_stmt.get())) == SQLITE_ROW) {
				call_back();
			}

			if(hresult != SQLITE_DONE) {
				errors::throw_sqlite_error(hresult, sql());
			}
			*/
		}

		void _extract_single_value(std::function<void(void)> call_back) {
		  /*
			int hresult;
			_start_execute();

			if((hresult = sqlite3_step(_stmt.get())) == SQLITE_ROW) {
				call_back();
			} else if(hresult == SQLITE_DONE) {
				throw errors::no_rows("no rows to extract: exactly 1 row expected", sql(), SQLITE_DONE);
			}

			if((hresult = sqlite3_step(_stmt.get())) == SQLITE_ROW) {
				throw errors::more_rows("not all rows extracted", sql(), SQLITE_ROW);
			}

			if(hresult != SQLITE_DONE) {
				errors::throw_sqlite_error(hresult, sql());
			}
			*/
		}



		/* for nullptr & unique_ptr support */
		friend database_binder& operator <<(database_binder& db, std::nullptr_t);
		template<typename T> friend void get_col_from_db(database_binder& db, int inx, std::unique_ptr<T>& val);
		template<typename T> friend T operator++(database_binder& db, int);
		// Overload instead of specializing function templates (http://www.gotw.ca/publications/mill17.htm)
		friend void get_col_from_db(database_binder& db, int inx, int& val);
		friend void get_col_from_db(database_binder& db, int inx, sqlite3_int64& i);
		friend void get_col_from_db(database_binder& db, int inx, float& f);
		friend void get_col_from_db(database_binder& db, int inx, double& d);
		friend void get_col_from_db(database_binder& db, int inx, std::string & s);
		friend database_binder&  append_string_argument(database_binder& db, const char *str,size_t size);
		friend void get_col_from_db(database_binder& db, int inx, std::u16string & w);

		template<typename Argument >
		  friend database_binder& operator<<(database_binder& db, Argument && val) ;

		template <typename OptionalT> friend database_binder& operator <<(database_binder& db, const optional<OptionalT>& val);
		template <typename OptionalT> friend void get_col_from_db(database_binder& db, int inx, optional<OptionalT>& o);


	public:
		database_binder(std::shared_ptr<MYSQL> db, std::string sql):
			_db(db),
			_sql(std::move(sql)),_unprepared_sql_part(_sql) {
			  consume_prepared_part();
		}

		~database_binder() noexcept(false) {
			if(!used() && !_has_uncaught_exception) {
				execute();
			}
		}

		void consume_prepared_part() {
		  if(_unprepared_sql_part.empty()) {
		    return;
		  }
		  auto pos=_unprepared_sql_part.find('?');
		  if(pos==_unprepared_sql_part.npos) {
		    _sql_stream.write(_unprepared_sql_part.data(),_unprepared_sql_part.size());
		    _unprepared_sql_part.remove_prefix(_unprepared_sql_part.size());
		  } else if(pos!=0) {
		    _sql_stream.write(_unprepared_sql_part.data(),pos);
		    _unprepared_sql_part.remove_prefix(pos);
		  }
		}

		/*
		template <typename Result>
		typename std::enable_if<is_sqlite_value<Result>::value, void>::type operator>>(
			Result& value) {
			this->_extract_single_value([&value, this] {
				get_col_from_db(*this, 0, value);
			});
		}
		*/

		template<typename... Types>
		void operator>>(std::tuple<Types...>&& values) {
			this->_extract_single_value([&values, this] {
				tuple_iterate<std::tuple<Types...>>::iterate(values, *this);
			});
		}

		/*
		template <typename Function>
		typename std::enable_if<!is_sqlite_value<Function>::value, void>::type operator>>(
			Function&& func) {
			typedef utility::function_traits<Function> traits;

			this->_extract([&func, this]() {
				binder<traits::arity>::run(*this, func);
			});
		}
		*/
	};

	namespace sql_function_binder {
		template<
			typename    ContextType,
			std::size_t Count,
			typename    Functions
		>
		inline void step(
				sqlite3_context* db,
				int              count,
				sqlite3_value**  vals
		);

		template<
			std::size_t Count,
			typename    Functions,
			typename... Values
		>
		inline typename std::enable_if<(sizeof...(Values) && sizeof...(Values) < Count), void>::type step(
				sqlite3_context* db,
				int              count,
				sqlite3_value**  vals,
				Values&&...      values
		);

		template<
			std::size_t Count,
			typename    Functions,
			typename... Values
		>
		inline typename std::enable_if<(sizeof...(Values) == Count), void>::type step(
				sqlite3_context* db,
				int,
				sqlite3_value**,
				Values&&...      values
		);

		template<
			typename    ContextType,
			typename    Functions
		>
		inline void final(sqlite3_context* db);

		template<
			std::size_t Count,
			typename    Function,
			typename... Values
		>
		inline typename std::enable_if<(sizeof...(Values) < Count), void>::type scalar(
				sqlite3_context* db,
				int              count,
				sqlite3_value**  vals,
				Values&&...      values
		);

		template<
			std::size_t Count,
			typename    Function,
			typename... Values
		>
		inline typename std::enable_if<(sizeof...(Values) == Count), void>::type scalar(
				sqlite3_context* db,
				int,
				sqlite3_value**,
				Values&&...      values
		);
	}
	

	struct mariadb_config{
	  	std::string host{"localhost"};
		unsigned int port;
	  	std::string user;
	  	std::string passwd;
		std::string unix_socket;
		unsigned long connect_flags{CLIENT_FOUND_ROWS};
	};

	class database {
	protected:
		std::shared_ptr<MYSQL> _db;

	public:
		database(const std::string &db_name, const mariadb_config &config = {}): _db(nullptr) {
			MYSQL* tmp = mysql_init(nullptr);
			if(!tmp)  {
			  throw std::runtime_error("mysql_init failed");
			}

			auto ret = mysql_real_connect(tmp,
			    config.host.c_str(),
			    config.user.c_str(),
			    config.passwd.c_str(),
			    db_name.c_str(),
			    config.port,
			    config.unix_socket.empty()?nullptr:config.unix_socket.c_str(),
			    config.connect_flags);
			_db = std::shared_ptr<MYSQL>(tmp, [=](MYSQL* ptr) {mysql_close(ptr); }); // this will close the connection eventually when no longer needed.
			if(!ret) errors::throw_mariadb_error(_db.get());
		}

		database(std::shared_ptr<MYSQL> db):
			_db(db) {}

		database_binder operator<<(const std::string& sql) {
			return database_binder(_db, sql);
		}

		database_binder operator<<(const char* sql) {
			return *this << std::string(sql);
		}

		auto connection() ->auto const { return _db; }

		my_ulonglong insert_id() const {
			return mysql_insert_id(_db.get());
		}
	};

	template<std::size_t Count>
	class binder {
	private:
		template <
			typename    Function,
			std::size_t Index
		>
			using nth_argument_type = typename utility::function_traits<
			Function
			>::template argument<Index>;

	public:
		// `Boundary` needs to be defaulted to `Count` so that the `run` function
		// template is not implicitly instantiated on class template instantiation.
		// Look up section 14.7.1 _Implicit instantiation_ of the ISO C++14 Standard
		// and the [dicussion](https://github.com/aminroosta/sqlite_modern_cpp/issues/8)
		// on Github.

		template<
			typename    Function,
			typename... Values,
			std::size_t Boundary = Count
		>
			static typename std::enable_if<(sizeof...(Values) < Boundary), void>::type run(
				database_binder& db,
				Function&&       function,
				Values&&...      values
				) {
			typename std::remove_cv<typename std::remove_reference<nth_argument_type<Function, sizeof...(Values)>>::type>::type value{};
			get_col_from_db(db, sizeof...(Values), value);

			run<Function>(db, function, std::forward<Values>(values)..., std::move(value));
		}

		template<
			typename    Function,
			typename... Values,
			std::size_t Boundary = Count
		>
			static typename std::enable_if<(sizeof...(Values) == Boundary), void>::type run(
				database_binder&,
				Function&&       function,
				Values&&...      values
				) {
			function(std::move(values)...);
		}
	};

	template<typename Argument >
	 inline database_binder& operator<<(database_binder& db, Argument && val) {
	   if(db._unprepared_sql_part.empty()) {
	     throw errors::more_prepare_arguments("no extra arguments needed to prepare sql",db._sql);
	   }
	   db._sql_stream<<std::forward<Argument>(val);
	   db._unprepared_sql_part.remove_prefix(1);
	   db.consume_prepared_part();
	   return db;
	}
	/*
	 inline void store_result_in_db(sqlite3_context* db, const int& val) {
		 sqlite3_result_int(db, val);
	}
	 inline void get_col_from_db(database_binder& db, int inx, int& val) {
		if(sqlite3_column_type(db._stmt.get(), inx) == SQLITE_NULL) {
			val = 0;
		} else {
			val = sqlite3_column_int(db._stmt.get(), inx);
		}
	}
	 inline void get_val_from_db(sqlite3_value *value, int& val) {
		if(sqlite3_value_type(value) == SQLITE_NULL) {
			val = 0;
		} else {
			val = sqlite3_value_int(value);
		}
	}

	 inline void store_result_in_db(sqlite3_context* db, const float& val) {
		 sqlite3_result_double(db, val);
	}
	 inline void get_col_from_db(database_binder& db, int inx, float& f) {
		if(sqlite3_column_type(db._stmt.get(), inx) == SQLITE_NULL) {
			f = 0;
		} else {
			f = float(sqlite3_column_double(db._stmt.get(), inx));
		}
	}
	 inline void get_val_from_db(sqlite3_value *value, float& f) {
		if(sqlite3_value_type(value) == SQLITE_NULL) {
			f = 0;
		} else {
			f = float(sqlite3_value_double(value));
		}
	}

	 inline void store_result_in_db(sqlite3_context* db, const double& val) {
		 sqlite3_result_double(db, val);
	}
	*/
	 inline void get_col_from_db(database_binder& db, int inx, double& d) {
	   /*
		if(sqlite3_column_type(db._stmt.get(), inx) == SQLITE_NULL) {
			d = 0;
		} else {
			d = sqlite3_column_double(db._stmt.get(), inx);
		}
		*/
	}
	 inline void get_val_from_db(sqlite3_value *value, double& d) {
	   /*
		if(sqlite3_value_type(value) == SQLITE_NULL) {
			d = 0;
		} else {
			d = sqlite3_value_double(value);
		}
		*/
	}

	/* for nullptr support */
	inline database_binder& operator <<(database_binder& db, std::nullptr_t) {
	   if(db._unprepared_sql_part.empty()) {
	     throw errors::more_prepare_arguments("no extra arguments needed to prepare sql",db._sql);
	   }
	   db._sql_stream<<"NULL";
	   db._unprepared_sql_part.remove_prefix(1);
	   db.consume_prepared_part();
	   return db;
	}
	 inline void store_result_in_db(sqlite3_context* db, std::nullptr_t) {
		 sqlite3_result_null(db);
	}

	/* for unique_ptr<T> support */
	template<typename T> inline void get_col_from_db(database_binder& db, int inx, std::unique_ptr<T>& _ptr_) {
	  /*
		if(sqlite3_column_type(db._stmt.get(), inx) == SQLITE_NULL) {
			_ptr_ = nullptr;
		} else {
			auto underling_ptr = new T();
			get_col_from_db(db, inx, *underling_ptr);
			_ptr_.reset(underling_ptr);
		}
		*/
	}
	template<typename T> inline void get_val_from_db(sqlite3_value *value, std::unique_ptr<T>& _ptr_) {
		if(sqlite3_value_type(value) == SQLITE_NULL) {
			_ptr_ = nullptr;
		} else {
			auto underling_ptr = new T();
			get_val_from_db(value, *underling_ptr);
			_ptr_.reset(underling_ptr);
		}
	}

	// std::string
	 inline void get_col_from_db(database_binder& db, int inx, std::string & s) {
	   /*
		if(sqlite3_column_type(db._stmt.get(), inx) == SQLITE_NULL) {
			s = std::string();
		} else {
			sqlite3_column_bytes(db._stmt.get(), inx);
			s = std::string(reinterpret_cast<char const *>(sqlite3_column_text(db._stmt.get(), inx)));
		}
		*/
	}
	 inline void get_val_from_db(sqlite3_value *value, std::string & s) {

	}

	// Convert char* to string to trigger op<<(..., const std::string )
	template<std::size_t N> inline database_binder& operator <<(database_binder& db, const char(&STR)[N]) { 
	  return append_string_argument(db,STR,N-1);
	}

	inline database_binder& operator <<(database_binder& db, const std::string& txt) {
	  return append_string_argument(db,txt.c_str(),txt.size());
	}
	inline database_binder&  append_string_argument(database_binder& db, const char *str,size_t size) {

	   if(db._unprepared_sql_part.empty()) {
	     throw errors::more_prepare_arguments("no extra arguments needed to prepare sql",db._sql);
	   }

	   std::string escaped_str(size*2+1,'\0');
	   auto real_size=mysql_real_escape_string(db._db.get(),escaped_str.data(),str,size);
	   escaped_str.resize(real_size);

	   db._sql_stream<<'"';
	   db._sql_stream.write(escaped_str.data(),escaped_str.size());
	   db._sql_stream<<'"';
	   db._unprepared_sql_part.remove_prefix(1);
	   db.consume_prepared_part();
	   return db;
	}

	 inline void store_result_in_db(sqlite3_context* db, const std::string& val) {
		 sqlite3_result_text(db, val.data(), -1, SQLITE_TRANSIENT);
	}

	 template<class Integral, class = std::enable_if<std::is_integral<Integral>::type>>
	 inline void store_result_in_db(sqlite3_context* db, const Integral& val) {
	//	 store_result_in_db(db, static_cast<sqlite3_int64>(val));
	}
	 template<class Integral, class = typename std::enable_if<std::is_integral<Integral>::value>::type>
	 inline void get_col_from_db(database_binder& db, int inx, Integral& val) {
		sqlite3_int64 i;
		get_col_from_db(db, inx, i);
		val = i;
	}
	 template<class Integral, class = typename std::enable_if<std::is_integral<Integral>::value>::type>
	 inline void get_val_from_db(sqlite3_value *value, Integral& val) {
		sqlite3_int64 i;
		get_val_from_db(value, i);
		val = i;
	}

	// std::optional support for NULL values
	template <typename OptionalT> inline database_binder& operator <<(database_binder& db, const optional<OptionalT>& val) {
		if(val) {
			return db << std::move(*val);
		} else {
			return db << nullptr;
		}
	}
	template <typename OptionalT> inline void store_result_in_db(sqlite3_context* db, const optional<OptionalT>& val) {
		if(val) {
			store_result_in_db(db, *val);
		}
		sqlite3_result_null(db);
	}

	template <typename OptionalT> inline void get_col_from_db(database_binder& db, int inx, optional<OptionalT>& o) {
	  /*
		if(sqlite3_column_type(db._stmt.get(), inx) == SQLITE_NULL) {
			o.reset();
		} else {
			OptionalT v;
			get_col_from_db(db, inx, v);
			o = std::move(v);
		}
		*/
	}
	template <typename OptionalT> inline void get_val_from_db(sqlite3_value *value, optional<OptionalT>& o) {
		if(sqlite3_value_type(value) == SQLITE_NULL) {
			o.reset();
		} else {
			OptionalT v;
			get_val_from_db(value, v);
			o = std::move(v);
		}
	}


	// Convert the rValue binder to a reference and call first op<<, its needed for the call that creates the binder (be carefull of recursion here!)
	template<typename T> database_binder&& operator << (database_binder&& db, const T& val) { db << val; return std::move(db); }

	namespace sql_function_binder {
		template<class T>
		struct AggregateCtxt {
			T obj;
			bool constructed = true;
		};

		template<
			typename ContextType,
			std::size_t Count,
			typename    Functions
		>
		inline void step(
				sqlite3_context* db,
				int              count,
				sqlite3_value**  vals
		) {
			auto ctxt = static_cast<AggregateCtxt<ContextType>*>(sqlite3_aggregate_context(db, sizeof(AggregateCtxt<ContextType>)));
			if(!ctxt) return;
			try {
				if(!ctxt->constructed) new(ctxt) AggregateCtxt<ContextType>();
				step<Count, Functions>(db, count, vals, ctxt->obj);
				return;
			} catch(sqlite_exception &e) {
				sqlite3_result_error_code(db, e.get_code());
				sqlite3_result_error(db, e.what(), -1);
			} catch(std::exception &e) {
				sqlite3_result_error(db, e.what(), -1);
			} catch(...) {
				sqlite3_result_error(db, "Unknown error", -1);
			}
			if(ctxt && ctxt->constructed)
				ctxt->~AggregateCtxt();
		}

		template<
			std::size_t Count,
			typename    Functions,
			typename... Values
		>
		inline typename std::enable_if<(sizeof...(Values) && sizeof...(Values) < Count), void>::type step(
				sqlite3_context* db,
				int              count,
				sqlite3_value**  vals,
				Values&&...      values
		) {
			typename std::remove_cv<
					typename std::remove_reference<
							typename utility::function_traits<
									typename Functions::first_type
							>::template argument<sizeof...(Values)>
					>::type
			>::type value{};
			get_val_from_db(vals[sizeof...(Values) - 1], value);

			step<Count, Functions>(db, count, vals, std::forward<Values>(values)..., std::move(value));
		}

		template<
			std::size_t Count,
			typename    Functions,
			typename... Values
		>
		inline typename std::enable_if<(sizeof...(Values) == Count), void>::type step(
				sqlite3_context* db,
				int,
				sqlite3_value**,
				Values&&...      values
		) {
			static_cast<Functions*>(sqlite3_user_data(db))->first(std::forward<Values>(values)...);
		}

		template<
			typename    ContextType,
			typename    Functions
		>
		inline void final(sqlite3_context* db) {
			auto ctxt = static_cast<AggregateCtxt<ContextType>*>(sqlite3_aggregate_context(db, sizeof(AggregateCtxt<ContextType>)));
			try {
				if(!ctxt) return;
				if(!ctxt->constructed) new(ctxt) AggregateCtxt<ContextType>();
				store_result_in_db(db,
						static_cast<Functions*>(sqlite3_user_data(db))->second(ctxt->obj));
			} catch(sqlite_exception &e) {
				sqlite3_result_error_code(db, e.get_code());
				sqlite3_result_error(db, e.what(), -1);
			} catch(std::exception &e) {
				sqlite3_result_error(db, e.what(), -1);
			} catch(...) {
				sqlite3_result_error(db, "Unknown error", -1);
			}
			if(ctxt && ctxt->constructed)
				ctxt->~AggregateCtxt();
		}

		template<
			std::size_t Count,
			typename    Function,
			typename... Values
		>
		inline typename std::enable_if<(sizeof...(Values) < Count), void>::type scalar(
				sqlite3_context* db,
				int              count,
				sqlite3_value**  vals,
				Values&&...      values
		) {
			typename std::remove_cv<
					typename std::remove_reference<
							typename utility::function_traits<Function>::template argument<sizeof...(Values)>
					>::type
			>::type value{};
			get_val_from_db(vals[sizeof...(Values)], value);

			scalar<Count, Function>(db, count, vals, std::forward<Values>(values)..., std::move(value));
		}

		template<
			std::size_t Count,
			typename    Function,
			typename... Values
		>
		inline typename std::enable_if<(sizeof...(Values) == Count), void>::type scalar(
				sqlite3_context* db,
				int,
				sqlite3_value**,
				Values&&...      values
		) {
			try {
				store_result_in_db(db,
						(*static_cast<Function*>(sqlite3_user_data(db)))(std::forward<Values>(values)...));
			} catch(sqlite_exception &e) {
				sqlite3_result_error_code(db, e.get_code());
				sqlite3_result_error(db, e.what(), -1);
			} catch(std::exception &e) {
				sqlite3_result_error(db, e.what(), -1);
			} catch(...) {
				sqlite3_result_error(db, "Unknown error", -1);
			}
		}
	}
}
