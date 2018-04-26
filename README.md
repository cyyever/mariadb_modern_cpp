mariadb modern cpp wrapper
====

This library is a lightweight modern wrapper around mariadb/mysql C api inspired by [sqlite_modern_cpp](https://github.com/SqliteModernCpp/sqlite_modern_cpp) and uses C++17.

```c++
#include<iostream>
#include <mariadb_modern_cpp.h>
using namespace mariadb;
using namespace std;

int main() {

   try {
      // creates a database file 'dbfile.db' if it does not exists.
      database db("dbfile.db");

      // executes the query and creates a 'user' table
      db <<
         "create table if not exists user ("
         "   _id integer primary key autoincrement not null,"
         "   age int,"
         "   name text,"
         "   weight real"
         ");";

      // inserts a new user record.
      // binds the fields to '?' .
      // note that only types allowed for bindings are :
      //      int ,long, long long, float, double
      //      string , u16string
      // mariadb3 only supports utf8 and utf16 strings, you should use std::string for utf8 and std::u16string for utf16.
      // note that u"my text" is a utf16 string literal of type char16_t * .
      db << "insert into user (age,name,weight) values (?,?,?);"
         << 20
         << u"bob"
         << 83.25;

      int age = 21;
      float weight = 68.5;
      string name = "jack";
      db << u"insert into user (age,name,weight) values (?,?,?);" // utf16 query string
         << age
         << name
         << weight;

      cout << "The new record got assigned id " << db.last_insert_rowid() << endl;

      // slects from user table on a condition ( age > 18 ) and executes
      // the lambda for each row returned .
      db << "select age,name,weight from user where age > ? ;"
         << 18
         >> [&](int age, string name, double weight) {
            cout << age << ' ' << name << ' ' << weight << endl;
         };

      // selects the count(*) from user table
      // note that you can extract a single culumn single row result only to : int,long,long,float,double,string,u16string
      int count = 0;
      db << "select count(*) from user" >> count;
      cout << "cout : " << count << endl;

      // you can also extract multiple column rows
      db << "select age, name from user where _id=1;" >> tie(age, name);
      cout << "Age = " << age << ", name = " << name << endl;

      // this also works and the returned value will be automatically converted to string
      string str_count;
      db << "select count(*) from user" >> str_count;
      cout << "scount : " << str_count << endl;
   }
   catch (exception& e) {
      cout << e.what() << endl;
   }
}
```

You can not execute multiple statements separated by semicolons in one go.

Additional flags
----
You can pass additional open flags to mariadb by using a config object:

```c++
mariadb_config config;
config.flags = OpenFlags::READONLY
database db("some_db", config);
int a;
// Now you can only read from db
auto ps = db << "select a from table where something = ? and anotherthing = ?" >> a;
config.flags = OpenFlags::READWRITE | OpenFlags::CREATE; // This is the default
config.encoding = Encoding::UTF16; // The encoding is respected only if you create a new database
database db2("some_db2", config);
// If some_db2 didn't exists before, it will be created with UTF-16 encoding.
```

Prepared Statements
----
It is possible to retain and reuse statments this will keep the query plan and in case of an complex query or many uses might increase the performance significantly.

```c++
database db(":memory:");

// if you use << on a mariadb::database you get a prepared statment back
// this will not be executed till it gets destroyed or you execute it explicitly
auto ps = db << "select a,b from table where something = ? and anotherthing = ?"; // get a prepared parsed and ready statment

// first if needed bind values to it
ps << 5;
int tmp = 8;
ps << tmp;

// now you can execute it with `operator>>` or `execute()`.
// If the statement was executed once it will not be executed again when it goes out of scope.
// But beware that it will execute on destruction if it wasn't executed!
ps >> [&](int a,int b){ ... };

// after a successfull execution the statment can be executed again, but the bound values are resetted.
// If you don't need the returned values you can execute it like this
ps.execute();

// To disable the execution of a statment when it goes out of scope and wasn't used
ps.used(true); // or false if you want it to execute even if it was used

// Usage Example:

auto ps = db << "insert into complex_table_with_lots_of_indices values (?,?,?)";
int i = 0;
while( i < 100000 ){
   ps << long_list[i++] << long_list[i++] << long_list[i++];
   ps.execute();
}
```

Shared Connections
----
If you need the handle to the database connection to execute mariadb3 commands directly you can get a managed shared_ptr to it, so it will not close as long as you have a referenc to it.

Take this example on how to deal with a database backup using mariadbs own functions in a save and modern way.
```c++
try {
   database backup("backup");		//Open the database file we want to backup to

   auto con = db.connection();   // get a handle to the DB we want to backup in our scope
                                 // this way we are sure the DB is open and ok while we backup

   // Init Backup and make sure its freed on exit or exceptions!
   auto state =
      std::unique_ptr<mariadb3_backup,decltype(&mariadb3_backup_finish)>(
      mariadb3_backup_init(backup.connection().get(), "main", con.get(), "main"),
      mariadb3_backup_finish
      );

   if(state) {
      int rc;
      // Each iteration of this loop copies 500 database pages from database db to the backup database.
      do {
         rc = mariadb3_backup_step(state.get(), 500);
         std::cout << "Remaining " << mariadb3_backup_remaining(state.get()) << "/" << mariadb3_backup_pagecount(state.get()) << "\n";
      } while(rc == mariadb_OK || rc == mariadb_BUSY || rc == mariadb_LOCKED);
   }
} // Release allocated resources.
```

Transactions
----
You can use transactions with `begin;`, `commit;` and `rollback;` commands.

```c++
db << "begin;"; // begin a transaction ...   
db << "insert into user (age,name,weight) values (?,?,?);"
   << 20
   << u"bob"
   << 83.25f;
db << "insert into user (age,name,weight) values (?,?,?);" // utf16 string
   << 21
   << u"jack"
   << 68.5;
db << "commit;"; // commit all the changes.

db << "begin;"; // begin another transaction ....
db << "insert into user (age,name,weight) values (?,?,?);" // utf16 string
   << 19
   << u"chirs"
   << 82.7;
db << "rollback;"; // cancel this transaction ...

```

Blob
----
Use `std::vector<T>` to store and retrieve blob data.  
`T` could be `char,short,int,long,long long, float or double`.

```c++
db << "CREATE TABLE person (name TEXT, numbers BLOB);";
db << "INSERT INTO person VALUES (?, ?)" << "bob" << vector<int> { 1, 2, 3, 4};
db << "INSERT INTO person VALUES (?, ?)" << "sara" << vector<double> { 1.0, 2.0, 3.0, 4.0};

vector<int> numbers_bob;
db << "SELECT numbers from person where name = ?;" << "bob" >> numbers_bob;

db << "SELECT numbers from person where name = ?;" << "sara" >> [](vector<double> numbers_sara){
    for(auto e : numbers_sara) cout << e << ' '; cout << endl;
};
```

NULL values
----
If you have databases where some rows may be null, you can use `std::unique_ptr<T>` to retain the NULL values between C++ variables and the database.

```c++
db << "CREATE TABLE tbl (id integer,age integer, name string, img blob);";
db << "INSERT INTO tbl VALUES (?, ?, ?, ?);" << 1 << 24 << "bob" << vector<int> { 1, 2 , 3};
unique_ptr<string> ptr_null; // you can even bind empty unique_ptr<T>
db << "INSERT INTO tbl VALUES (?, ?, ?, ?);" << 2 << nullptr << ptr_null << nullptr;

db << "select age,name,img from tbl where id = 1"
		>> [](unique_ptr<int> age_p, unique_ptr<string> name_p, unique_ptr<vector<int>> img_p) {
			if(age_p == nullptr || name_p == nullptr || img_p == nullptr) {
				cerr << "ERROR: values should not be null" << std::endl;
			}

			cout << "age:" << *age_p << " name:" << *name_p << " img:";
			for(auto i : *img_p) cout << i << ","; cout << endl;
		};

db << "select age,name,img from tbl where id = 2"
		>> [](unique_ptr<int> age_p, unique_ptr<string> name_p, unique_ptr<vector<int>> img_p) {
			if(age_p != nullptr || name_p != nullptr || img_p != nullptr) {
				cerr << "ERROR: values should be nullptr" << std::endl;
				exit(EXIT_FAILURE);
			}

			cout << "OK all three values are nullptr" << endl;
		};
```

NULL values (C++17)
----
You can use `std::optional<T>` as an alternative for `std::unique_ptr<T>` to work with NULL values.

```c++
#include <mariadb_modern_cpp.h>

struct User {
   long long _id;
   std::optional<int> age;
   std::optional<string> name;
   std::optional<real> weight;
};

int main() {
   User user;
   user.name = "bob";

   // Same database as above
   database db("dbfile.db");

   // Here, age and weight will be inserted as NULL in the database.
   db << "insert into user (age,name,weight) values (?,?,?);"
      << user.age
      << user.name
      << user.weight;
   user._id = db.last_insert_rowid();

   // Here, the User instance will retain the NULL value(s) from the database.
   db << "select _id,age,name,weight from user where age > ? ;"
      << 18
      >> [&](long long id,
         std::optional<int> age,
         std::optional<string> name
         std::optional<real> weight) {

      cout << "id=" << _id
         << " age = " << (age ? to_string(*age) ? string("NULL"))
         << " name = " << (name ? *name : string("NULL"))
         << " weight = " << (weight ? to_string(*weight) : string(NULL))
         << endl;
   };
}
```

Errors
----

On error, the library throws an error class indicating the type of error. The error classes are derived from the mariadb3 error names, so if the error code is mariadb_CONSTRAINT, the error class thrown is mariadb::errors::constraint. mariadb3 extended error names are supported too. So there is e.g. a class mariadb::errors::constraint_primarykey derived from mariadb::errors::constraint. Note that all errors are derived from mariadb::mariadb_exception and that itself is derived from std::runtime_exception.
mariadb::mariadb_exception has a `get_code()` member function to get the mariadb3 error code or `get_extended_code()` to get the extended error code.
Additionally you can use `get_sql()` to see the SQL statement leading to the error.

```c++
database db(":memory:");
db << "create table person (id integer primary key not null, name text);";

try {
   db << "insert into person (id, name) values (?,?)" << 1 << "jack";
   // inserting again to produce error
   db << "insert into person (id, name) values (?,?)" << 1 << "jack";
}
/* if you are trying to catch all mariadb related exceptions
 * make sure to catch them by reference */
catch (mariadb_exception& e) {
   cerr  << e.get_code() << ": " << e.what() << " during "
         << e.get_sql() << endl;
}
/* you can catch specific exceptions as well,
   catch(mariadb::errors::constraint e) {  } */
/* and even more specific exceptions
   catch(mariadb::errors::constraint_primarykey e) {  } */
```

You can also register a error logging function with `mariadb::error_log`.
The `<mariadb_modern_cpp/log.h>` header has to be included to make this function available.
The call to `mariadb::error_log` has to be the first call to any `mariadb_modern_cpp` function by your program.

```c++
error_log(
   [&](mariadb_exception& e) {
      cerr  << e.get_code() << ": " << e.what() << endl;
   },
   [&](errors::misuse& e) {
      /* You can behave differently to specific errors */
   }
);
database db(":memory:");
db << "create table person (id integer primary key not null, name text);";

try {
   db << "insert into person (id, name) values (?,?)" << 1 << "jack";
   // inserting again to produce error
   db << "insert into person (id, name) values (?,?)" << 1 << "jack";
}
catch (mariadb_exception& e) {}
```

Building and Installing
----

The usual way works for installing:

```bash
mkdir build && cmake .. && make && sudo make install
```
