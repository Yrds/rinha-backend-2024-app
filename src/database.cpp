#include "database.hpp"
#include "sqlite3.h"
#include <stdexcept>

#include <iostream>

namespace database {

struct Connection {
  sqlite3 *db = nullptr;
  sqlite3_stmt* stmt = nullptr;
  const bool transactional;
  int rc = SQLITE_OK;

  Connection(bool transactional): transactional(transactional) {
    rc = sqlite3_open("database.db", &db);

    if(rc != SQLITE_OK) {
      throw std::runtime_error("DATABASE couldn't be created");
    }

    sqlite3_busy_timeout(db, 1000);

    if(transactional) {
      sqlite3_exec(db, "BEGIN IMMEDIATE;", 0, 0, 0);
    }
  }

  template<typename Functor>
  void command(Functor functor) {
    if(rc == SQLITE_DONE
        || rc == SQLITE_ROW
        || rc == SQLITE_OK) {
      rc = functor();
      std::cout << rc << std::endl;
    }
  }

  template<typename... Functors>
  void command(Functors&&... functors) {
    ([&]{
     command(functors);
    } (), ...);
  }

};

void deleteConnection(Connection* connection) {
  connection->rc = sqlite3_finalize(connection->stmt);
  connection->rc = sqlite3_close(connection->db);

  std::cout << "Closing with exit code: " << connection->rc  << std::endl;

  if(connection->rc == SQLITE_OK && connection->transactional) {
    sqlite3_exec(connection->db, "COMMIT;", 0, 0, 0);
    std::cout << "Commit" << std::endl;
    return;
  }

  if(connection->transactional) {
    std::cout << "Rollback" << std::endl;
    sqlite3_exec(connection->db, "ROLLBACK;", 0, 0, 0);
  }
};

std::unique_ptr<Connection, void(*)(Connection*)> getConnection(bool transactional) {
  return std::unique_ptr<Connection, void (*)(Connection *)>(new Connection(transactional), deleteConnection);
}

void run_stmt(Connection* connection, const char* sql) {
    char *zErrMsg = 0;

    int rc = sqlite3_exec(connection->db, sql, nullptr, 0, &zErrMsg);

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

    connection->command(
        [&]() { return sqlite3_prepare_v2(connection->db, sql, 256, &connection->stmt, nullptr); },
        [&]() { return sqlite3_bind_int(connection->stmt, 1, clientId); },
        [&]() { return sqlite3_step(connection->stmt); }
    );

    if(connection->rc == SQLITE_DONE) {
      return std::unexpected(UNEXPECTED_CODE::NOT_FOUND);
    }

    if(connection->rc == SQLITE_ROW) {
      models::Extract extract;

      extract.saldo.total = sqlite3_column_int(connection->stmt, 0);
      extract.saldo.limite = sqlite3_column_int(connection->stmt, 1);
      extract.saldo.data_extrato = std::chrono::system_clock::now();

      return extract;
    }

    return std::unexpected(UNEXPECTED_CODE::UNKNOWN);
}

std::expected<std::vector<models::TransactionHistory>, UNEXPECTED_CODE>
getLastTransactionsByClientId(Connection* connection, int clientId) {

    auto sql = R"(
      SELECT valor, tipo, descricao, unixepoch(realizada_em, 'subsec') * 1000
      FROM transacoes
      WHERE cliente_id = ?
      ORDER BY realizada_em DESC
      LIMIT 10;
    )";

    connection->command([&]() {
      return sqlite3_prepare_v2(connection->db, sql, 256, &connection->stmt, nullptr);
    });

    connection->command([&]() {
      return sqlite3_bind_int(connection->stmt, 1, clientId);
    });


    connection->command([&]() {
      return sqlite3_step(connection->stmt);
    });

    std::vector<models::TransactionHistory> transactionHistory;

    while(connection->rc == SQLITE_ROW) {
      std::basic_string<unsigned char> type {sqlite3_column_text(connection->stmt, 1)};
      std::string description{reinterpret_cast<const char *>(sqlite3_column_text(connection->stmt, 2))};
      transactionHistory.push_back(models::TransactionHistory{
        sqlite3_column_int(connection->stmt, 0),
        static_cast<models::TRANSACTION_TYPE>(type.at(0)),
        description,
        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>{
            std::chrono::milliseconds{sqlite3_column_int(connection->stmt, 3)}
            }
      });

      connection->command([&]() {
        return sqlite3_step(connection->stmt);
      });
    }


    if(connection->rc == SQLITE_OK) {
      return transactionHistory;
    }

    return std::unexpected(UNEXPECTED_CODE::UNKNOWN);
}

std::expected<models::TransactionResponse, UNEXPECTED_CODE>
createTransaction(Connection* connection, const int clientId, const models::Transaction& transaction, models::TransactionResponse& balance) {

    auto sql = R"(
      INSERT INTO transacoes(cliente_id, valor, tipo, descricao, realizada_em)
      values(?, ?, ?, ?, ?);
    )";

    connection->command([&]() {
      return sqlite3_prepare_v2(connection->db, sql, 256, &connection->stmt, nullptr);
    });

    connection->command([&]() {
      return sqlite3_bind_int(connection->stmt, 1, clientId);
    });

    connection->command([&]() {
      return sqlite3_bind_int(connection->stmt, 2, transaction.valor);
    });

    const std::string type{static_cast<char>(transaction.tipo)};

    connection->command([&]() {
      return sqlite3_bind_text(connection->stmt, 3, type.c_str(), 1, SQLITE_STATIC);
    });

    connection->command([&]() {
      return sqlite3_bind_text(connection->stmt, 4, transaction.descricao.c_str(), transaction.descricao.size(), SQLITE_STATIC);
    });

    const std::string transactionTimeFormated = std::format("{0:%FT%TZ}", std::chrono::system_clock::now());

    connection->command([&]() {
      return sqlite3_bind_text(connection->stmt, 5, transactionTimeFormated.c_str(), transactionTimeFormated.size(), SQLITE_STATIC);
    });

    connection->command([&]() {
      return sqlite3_step(connection->stmt);
    });

    const auto debitSql = "UPDATE saldos SET valor=? where cliente_id = ?;";

    connection->command([&]() {
      return sqlite3_prepare_v2(connection->db, debitSql, 256, &connection->stmt, nullptr);
    });

    if (connection->rc == SQLITE_ERROR) {


      return std::unexpected(UNEXPECTED_CODE::UNKNOWN);
    }

    connection->command([&]() {
      return sqlite3_bind_int(connection->stmt, 1, balance.saldo);
    });

    connection->command([&]() {
      return sqlite3_bind_int(connection->stmt, 2, clientId);
    });

    connection->command([&]() {
      return sqlite3_step(connection->stmt);
    });

    if(connection->rc == SQLITE_DONE) {
      return balance;
    }

    return std::unexpected(UNEXPECTED_CODE::UNKNOWN);
}

std::expected<models::TransactionResponse, UNEXPECTED_CODE>
getBalance(Connection *connection, const int clientId) {

  auto sql = R"(
    SELECT s.valor as total, c.limite as limite
    FROM clientes as c,
    saldos as s WHERE cliente_id = ? AND s.cliente_id = c.id LIMIT 1;)";

  connection->command([&]() {
    return sqlite3_prepare_v2(connection->db, sql, 256, &(connection->stmt), nullptr);
  });

  connection->command([&]() { return sqlite3_bind_int(connection->stmt, 1, clientId); });

  connection->command([&]() { return sqlite3_step(connection->stmt); });

  if (connection->rc == SQLITE_DONE) {
    return std::unexpected(UNEXPECTED_CODE::NOT_FOUND);
  }

  if (connection->rc == SQLITE_ROW) {
    models::TransactionResponse balance;

    balance.saldo = sqlite3_column_int(connection->stmt, 0);
    balance.limite = sqlite3_column_int(connection->stmt, 1);

    return balance;
  }

  return std::unexpected(UNEXPECTED_CODE::UNKNOWN);
}

}
