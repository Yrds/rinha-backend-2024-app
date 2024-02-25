#include "database.hpp"
#include "sqlite3.h"

#include <iostream>

namespace database {

struct Connection {
  sqlite3 *db;

  Connection() {
    sqlite3_open("database.db", &db);
  }

};

void deleteConnection(Connection* connection) {
  sqlite3_close(connection->db);
};

std::unique_ptr<Connection, void(*)(Connection*)> getConnection() {
  return std::unique_ptr<Connection, void (*)(Connection *)>(new Connection(), deleteConnection);
}

void run_stmt(Connection* connection, const char* sql) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(connection->db, sql, 1024, &stmt, nullptr);

    int rc;

    char *zErrMsg = 0;

    rc = sqlite3_exec(connection->db, sql, nullptr, 0, &zErrMsg);

    if(rc != SQLITE_OK) {
      std::cerr << zErrMsg << std::endl;
      sqlite3_free(zErrMsg);
    }
}

models::Extract getExtractByClientId(Connection* connection, int clientId) {
    sqlite3_stmt* stmt;

    auto sql = "SELECT valor, data_extrato, limite FROM saldos WHERE cliente_id = ?";
    sqlite3_prepare_v2(connection->db, sql, 1024, &stmt, nullptr);

    sqlite3_bind_int(stmt, 0, clientId);

    int rc;

    char *zErrMsg = 0;

    models::Extract extract; 

    rc = sqlite3_exec(connection->db, sql, [](void *extractPtr, int argc, char **argv, char **azColName) -> int {
          models::Extract* extract = static_cast<models::Extract*>(extractPtr);

          int i;
          for (i = 0; i < argc; i++) {
            printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
          }
          printf("\n");

          return 0;
        }, 0, &zErrMsg);

    if(rc != SQLITE_OK) {
      std::cerr << zErrMsg << std::endl;
      sqlite3_free(zErrMsg);
    }

    return models::Extract();
}

}
