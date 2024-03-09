#include "database.hpp"
#include "sqlite3.h"
#include <stdexcept>

#include <iostream>

namespace database {

struct Connection {
  sqlite3 *db = nullptr;

  Connection() {
    int rc = sqlite3_open("database.db", &db);
    if(rc != SQLITE_OK) {
      throw std::runtime_error("DATABASE couldn't be created");
    }
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

std::expected<models::Extract, UNEXPECTED_CODE>
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
      return std::unexpected(UNEXPECTED_CODE::NOT_FOUND);
    }

    if(rc == SQLITE_ROW) {
      models::Extract extract;

      extract.saldo.total = sqlite3_column_int(stmt, 0);
      extract.saldo.limite = sqlite3_column_int(stmt, 1);
      extract.saldo.data_extrato = std::chrono::system_clock::now(),

      sqlite3_finalize(stmt);
      return extract;
    }

    sqlite3_finalize(stmt);
    return std::unexpected(UNEXPECTED_CODE::UNKNOWN);
}

std::expected<std::vector<models::TransactionHistory>, UNEXPECTED_CODE>
getLastTransactionsByClientId(Connection* connection, int clientId) {

    auto sql = R"(
      SELECT valor, tipo, descricao, realizada_em
      FROM transacoes
      WHERE cliente_id = ?
      LIMIT 10;
    )";

    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(connection->db, sql, 256, &stmt, nullptr);

    int rc = sqlite3_bind_int(stmt, 1, clientId);

    rc = sqlite3_step(stmt);

    std::vector<models::TransactionHistory> transactionHistory;

    while(rc == SQLITE_ROW) {
      std::basic_string<unsigned char> type {sqlite3_column_text(stmt, 1)};
      std::string description{reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2))};
      transactionHistory.push_back(models::TransactionHistory{
        sqlite3_column_int(stmt, 0),
        static_cast<models::TRANSACTION_TYPE>(type.at(0)),
        description,
        std::chrono::system_clock::now() //TODO get description from database
      });

      rc = sqlite3_step(stmt);
    }

    sqlite3_finalize(stmt);

    if(rc == SQLITE_DONE) {
      return transactionHistory;
    }

    return std::unexpected(UNEXPECTED_CODE::UNKNOWN);
}

std::expected<models::TransactionResponse, std::string>
createTransaction(Connection* connection, const int clientId, const models::Transaction& transaction) {
    int rc = sqlite3_exec(connection->db, "BEGIN TRANSACTION;", 0, 0, 0);

    auto sql = R"(
      INSERT INTO transacoes(cliente_id, valor, tipo, descricao, realizada_em)
      values(?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;

    rc = sqlite3_prepare_v2(connection->db, sql, 256, &stmt, nullptr);

    rc = sqlite3_bind_int(stmt, 1, clientId);
    rc = sqlite3_bind_int(stmt, 2, transaction.valor);

    const std::string type{static_cast<char>(transaction.tipo)};
    rc = sqlite3_bind_text(stmt, 3, type.c_str(), 1, SQLITE_STATIC);

    rc = sqlite3_bind_text(stmt, 4, transaction.descricao.c_str(), transaction.descricao.size(), SQLITE_STATIC);

    const std::string transactionTimeFormated = std::format("{0:%FT%TZ}", std::chrono::system_clock::now());
    rc = sqlite3_bind_text(stmt, 5, transactionTimeFormated.c_str(), transactionTimeFormated.size(), SQLITE_STATIC);

    rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    //if (transaction.tipo == models::TRANSACTION_TYPE::DEBIT) {
    //  const auto debitSql = "UPDATE saldos SET valor=? where client_id = ?;";
    //  rc = sqlite3_prepare_v2(connection->db, debitSql, 256, stmt, nullptr);

    //  //TODO
    //  rc = sqlite3_bind_int(stmt, 1, transaction.valor);
    //  rc = sqlite3_bind_int(stmt, 2, 0);
    //}

    if(rc == SQLITE_DONE) {
      return models::TransactionResponse();
    } else {
      sqlite3_exec(connection->db, "END TRANSACTION;", 0, 0, 0);
    }

    return std::unexpected("Unknown error " + std::to_string(rc));
}



}
