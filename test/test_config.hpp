#include "../hdr/mariadb_modern_cpp.hpp"

inline mariadb::mariadb_config get_test_config() {
  mariadb::mariadb_config config;
  config.host = getenv("mariadb_host");
  if (!config.host) {
    config.host = "127.0.0.1";
  }
  config.port = 3306;
  config.user = "mariadb_modern_cpp_test";
  config.passwd = "123";
  config.default_database = "mariadb_modern_cpp_test";

  return config;
}
