drop database if exists mariadb_modern_cpp_test;
create database if not exists mariadb_modern_cpp_test;
GRANT ALL PRIVILEGES ON mariadb_modern_cpp_test.* TO 'mariadb_modern_cpp_test'@'%' IDENTIFIED BY '123';
flush privileges;

create table if not exists mariadb_modern_cpp_test.col_type_test (
  id BIGINT PRIMARY KEY AUTO_INCREMENT NOT NULL,
  int_col BIGINT NOT NULL,
  uint_col BIGINT UNSIGNED NOT NULL,
  dec_col DECIMAL(5,2) NOT NULL,
  udec_col DECIMAL(5,2) UNSIGNED NOT NULL,
  double_col DOUBLE NOT NULL,
  udouble_col DOUBLE UNSIGNED NOT NULL,
  varchar_col VARCHAR(10) NOT NULL,
  char_col CHAR(4) NOT NULL,
  longtext_col LONGTEXT NOT NULL,
  longblob_col LONGBLOB NOT NULL,
  null_col LONGTEXT
);

insert into mariadb_modern_cpp_test.col_type_test(int_col,uint_col,dec_col,udec_col,double_col,udouble_col,varchar_col,char_col,longtext_col,longblob_col,null_col) values(-1,1,-0.3,0.3,-0.3,0.3,"varchar","char","longtext","longblob",NULL);
