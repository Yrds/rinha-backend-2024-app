#include "database.hpp"
#include "sqlite3.h"

#include <iostream>

namespace database {

struct Connection {
  sqlite3 *db;

  Connection() {
    sqlite3_open("database.db", &db);

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, "SELECT * FROM test", 1024, &stmt, nullptr);
    while(sqlite3_step(stmt) != SQLITE_DONE) {
      std::cout << sqlite3_column_int(stmt, 0) << std::endl;
    }
    sqlite3_finalize(stmt);
  }

};

void deleteConnection(Connection* connection) {
  sqlite3_close(connection->db);
};

std::unique_ptr<Connection, void(*)(Connection*)> getConnection() {
  return std::unique_ptr<Connection, void (*)(Connection *)>(new Connection(), deleteConnection);
}

}
