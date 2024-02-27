#include <iostream>
#include <fstream>
#include <filesystem>
#include <format>

#include "httplib.h"
#include "nlohmann/json.hpp"
#include "database.hpp"

#define PROJECT_NAME "rinha-2024-q1-project"

void
extract(const httplib::Request &req, httplib::Response &res) {
    auto connection = database::getConnection();

    auto clientId = std::stoi(req.path_params.at("id"));

    auto result = database::getExtractByClientId(connection.get(), clientId);

    if (result.has_value()) {

      auto& extract = *result;

      auto transactionResult = database::getLastTransactionsByClientId(connection.get(), clientId);

      if(transactionResult.has_value()) {
        extract.ultimas_transacoes = *transactionResult;
      }

      nlohmann::json data;
      data["saldo"]["data_extrato"] = std::format("{0:%FT%TZ}", extract.saldo.data_extrato);
      data["saldo"]["limite"] = extract.saldo.limite;
      data["saldo"]["total"] = extract.saldo.total;
      data["saldo"]["ultimas_transacoes"] = nlohmann::json::array();

      for(const auto& transaction: extract.ultimas_transacoes) {
        nlohmann::json transactionData;

        transactionData["valor"] = transaction.valor;
        transactionData["tipo"] = transaction.tipo;
        transactionData["descricao"] = transaction.descricao;
        transactionData["realizada_em"] = std::format("{0:%FT%TZ}", transaction.realizada_em);

        data["saldo"]["ultimas_transacoes"].push_back(transactionData);
      }

      res.set_content(data.dump(), "application/json");
      return;
    }

    if(result.error() == "Not found") {
      res.status = 404;
    }

    res.set_content(result.error(), "text/html");
}


void initDatabase() {
    namespace fs = std::filesystem;

    fs::remove("database.db"); 

    std::fstream s{"init.sql", s.in};

    if(!s.is_open()) {
        return;
    }

    auto connection = database::getConnection();

    std::stringstream sql;

    sql << s.rdbuf();

    database::run_stmt(connection.get(), sql.str().c_str());
}

int
main(int argc, char **argv) {

    //initDatabase();
    // HTTP
    httplib::Server svr;

    svr.Get(R"(/clientes/:id/extrato)", extract);

    svr.listen("0.0.0.0", 8080);
    return 0;
}
