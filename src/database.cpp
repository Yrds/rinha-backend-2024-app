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

std::expected<models::Extract, std::string>
getExtractByClientId(Connection* connection, int clientId) {

    auto sql = R"(
      SELECT s.valor as total, c.limite as limite
      FROM clientes as c, saldos as s
      WHERE cliente_id = ? AND s.cliente_id = c.id
      LIMIT 1;
    )";

    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(connection->db, sql, 256, &stmt, nullptr);

    int rc = sqlite3_bind_int(stmt, 1, clientId);

    rc = sqlite3_step(stmt);

    if(rc == SQLITE_DONE) {
      sqlite3_finalize(stmt);
      return std::unexpected("Not found");
    }

    if(rc == SQLITE_ROW) {
      models::Extract extract;

      extract.saldo.total = sqlite3_column_int(stmt, 0);
      extract.saldo.limite = sqlite3_column_int(stmt, 1);
      extract.saldo.data_extrato = std::chrono::system_clock::now(),
      extract.ultimas_transacoes = {};

      return extract;
      sqlite3_finalize(stmt);
    }

    sqlite3_finalize(stmt);

    return std::unexpected("Unknown error");
}


}
