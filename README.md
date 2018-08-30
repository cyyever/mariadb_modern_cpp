mariadb modern cpp wrapper
====

This library is a lightweight modern wrapper around mariadb/mysql C api inspired by [sqlite_modern_cpp](https://github.com/SqliteModernCpp/sqlite_modern_cpp) and uses C++17.

```c++
#include <iostream>
#include <mariadb_modern_cpp.hpp>
using namespace mariadb;
using namespace std;

int main() {

   try {
      // connects to server
      mariadb::mariadb_config config;
      config.host = "xxx";
      config.port = 3306;
      config.user = "xxx";
      config.passwd = "xxx";
      config.default_database = "my_db";
      database db(config);

      // executes the query and creates a 'user' table
      db <<
         "create table if not exists user ("
         "   _id int primary key autoincrement not null,"
         "   age int,"
         "   name text,"
         "   weight real"
         ");";

      // inserts a new user record.
      // binds the fields to '?' .
      // note that only types allowed for bindings are :
      //      integer types(int, long, long long, etc)
      //      float types(float , double , etc)
      //      string (for CHAR,VARCHAR,TEXT columns)
      //      std::vector<std::byte> (for BLOB columns)
      //      std::optional (for NULL columns)
      db << "insert into user (age,name,weight) values (?,?,?);"
         << 20
         << "bob"
         << 83.25;

      int age = 21;
      float weight = 68.5;
      string name = "jack";
      db << "insert into user (age,name,weight) values (?,?,?);"
         << age
         << name
         << weight;

      //  gets the value generated for an AUTO_INCREMENT column by the previous INSERT or UPDATE statement.
      cout << "The new record got assigned id " << db.insert_id() << endl;

      // selects from user table on a condition ( age > 18 ) and executes
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

NOTES
----
Currently multi-statement execution is not supported.

Prepared Statements
----
It is possible to retain and reuse statments this will keep the query plan and in case of an complex query or many uses might increase the performance significantly.

```c++
database db(config);

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
If you need the handle to the database connection to execute mariadb commands directly you can get a managed shared_ptr to it, so it will not close as long as you have a referenc to it.

Take this example on how to deal with a database backup using mariadbs own functions in a save and modern way.
```c++
try {
   database db(config);

   auto mysql_handle = db.connection();   // get a handle to the DB we want to backup in our scope
                                 // this way we are sure the DB is open and ok while we backup

   mysql_ping(mysql_handle.get());
} // Release allocated resources.
```

Transactions
----
All sql statements executed in a transaction context are commited as a transaction when the context is destructed,but if an exception is thrown,the transaction is canceled.

```c++
{
  auto ctx = db.get_transaction_context();
  db << "insert into user (age,name,weight) values (?,?,?);"
     << 20
     << "bob"
     << 83.25f;
  db << "insert into user (age,name,weight) values (?,?,?);"
     << 19
     << "chirs"
     << 82.7;
}

{
  auto ctx = db.get_transaction_context();
  db << "insert into user (age,name,weight) values (?,?,?);"
     << 20
     << "bob"
     << 83.25f;
  db << "insert into user (age,name,weight) values (?,?,?);"
     << 19
     << "chirs"
     << 82.7;
  throw std::runtime_error("Transaction will be canceled");
}

```

Blob
----
Use `std::vector<std::byte>` to store and retrieve blob data.  

You can also Use `std::vector<T>` to store and retrieve blob data where `T` is a integer type,we will treat the vector as an continuous memeory block like std::vector<std::byte>.

```c++
db << "CREATE TABLE person (name TEXT, numbers BLOB);";
db << "INSERT INTO person VALUES (?, ?)" << "bob" << vector<std::byte> { 1, 2, 3, 4};

vector<std::byte> numbers_bob;
db << "SELECT numbers from person where name = ?;" << "bob" >> numbers_bob;

db << "SELECT numbers from person where name = ?;" << "sara" >> [](vector<std::byte> numbers_sara){
    for(auto e : numbers_sara) cout << (int)e << ' '; cout << endl;
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

You can use `std::optional<T>` as an alternative for `std::unique_ptr<T>` to work with NULL values.

```c++
#include <mariadb_modern_cpp.hpp>

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
   database db(config);

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

On error, the library throws an error class indicating the type of error. The error classes are derived from the mariadb::mariadb_exception class.
you can use `get_sql()` to see the SQL statement leading to the error.

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
   cerr  << e.what() << " during "
         << e.get_sql() << endl;
}
```

Building and Installing
----

The usual way works for installing:

```bash
mkdir build && cmake .. && make && sudo make install
```
