/*!
 * \file face_bound_test.cpp
 *
 * \author cyy
 * \date 2017-12-29
 */

#include "../hdr/mariadb_modern_cpp.hpp"

static std::unique_ptr<mariadb::database> &get_database() {
  static std::unique_ptr<mariadb::database> db;

  if (!db) {
    mariadb::mariadb_config config;
    config.host = "127.0.0.1";
    config.user = "mariadb_modern_cpp_test";
    config.passwd = "123";
    config.default_database = "mariadb_modern_cpp_test";

    db = std::make_unique<mariadb::database>(config);
  }
  return db;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  if (Size == 0) {
    return 0;
  }

  try {
    (*get_database()) << std::string(reinterpret_cast<const char *>(Data),
                                     Size);
  } catch (const mariadb::mariadb_exception &) {
  }
  return 0; // Non-zero return values are reserved for future use.
}
