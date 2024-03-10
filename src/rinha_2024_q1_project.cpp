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

    auto search = req.path_params.find("id");

    if (search == req.path_params.end()) {
      res.status = 422;
      return;
    }

    auto clientId = std::stoi(search->second);

    auto connection = database::getConnection(true);

    auto result = database::getExtractByClientId(connection.get(), clientId);

    if (!result.has_value()) {
      switch(result.error()) {
        case UNEXPECTED_CODE::NOT_FOUND:
          res.status = 404;
          break;
        case UNEXPECTED_CODE::UNKNOWN:
          res.status = 500;
          break;
      }

      return;
    }

    auto &extract = *result;

    auto transactionResult =
        database::getLastTransactionsByClientId(connection.get(), clientId);

    if (transactionResult.has_value()) {
      extract.ultimas_transacoes = *transactionResult;
    }

    nlohmann::json data;
    data["saldo"]["data_extrato"] =
        std::format("{0:%FT%TZ}", extract.saldo.data_extrato);
    data["saldo"]["limite"] = extract.saldo.limite;
    data["saldo"]["total"] = extract.saldo.total;
    data["saldo"]["ultimas_transacoes"] = nlohmann::json::array();

    for (const auto &transaction : extract.ultimas_transacoes) {
      nlohmann::json transactionData;

      transactionData["valor"] = transaction.valor;
      transactionData["tipo"] = std::string{static_cast<char>(transaction.tipo)};
      transactionData["descricao"] = transaction.descricao;
      transactionData["realizada_em"] =
          std::format("{0:%FT%TZ}", transaction.realizada_em);

      data["saldo"]["ultimas_transacoes"].push_back(transactionData);
    }

    res.status = 200;
    res.set_content(data.dump(), "application/json");
}

void
createTransaction(const httplib::Request &req, httplib::Response &res) {
  nlohmann::json data = nlohmann::json::parse(req.body, nullptr, false);

  if (data.is_discarded()) {
      res.status = 422;
      res.set_content("", "text/html");
      return;
  }

  if(data["tipo"].empty() || !data["tipo"].is_string()) {
      res.status = 422;
      res.set_content("", "text/html");
      return;
  }

  if(data["valor"].empty() || !data["valor"].is_number_integer()) {
      res.status = 422;
      res.set_content("", "text/html");
      return;
  }

  if(data["descricao"].empty() || !data["descricao"].is_string()) {
      res.status = 422;
      res.set_content("", "text/html");
      return;
  }

  auto clientId = std::stoi(req.path_params.at("id"));

  std::basic_string<unsigned char> transactionType {reinterpret_cast<const unsigned char*>(
         data["tipo"].template get<std::string>().c_str()
         )};

  const models::Transaction transaction {
   data["valor"].template get<int>(),
   static_cast<models::TRANSACTION_TYPE>(transactionType.at(0)),
   data["descricao"].template get<std::string>()
  };

  if (transaction.valor <= 0) {
    res.status = 422;
    res.set_content("", "text/html");
    return;
  }

  if (transaction.tipo != models::TRANSACTION_TYPE::DEBIT &&
      transaction.tipo != models::TRANSACTION_TYPE::CREDIT) {
    res.status = 422;
    res.set_content("", "text/html");
    return;
  }

  if (transaction.descricao.size() >= 10 || transaction.descricao.size() == 0) {
    res.status = 422;
    res.set_content("", "text/html");
    return;
  }

  auto connection = database::getConnection(true);

  auto balanceResult = database::getBalance(connection.get(), clientId);

  if (!balanceResult.has_value()) {
    switch(balanceResult.error()) {
      case UNEXPECTED_CODE::NOT_FOUND:
        res.status = 404;
        break;
      case UNEXPECTED_CODE::UNKNOWN:
        res.status = 500;
        break;
    }

    res.set_content("", "text/html");
    return;
  }

  auto balance = balanceResult.value();

  balance.saldo = balance.saldo + (
      transaction.tipo == models::TRANSACTION_TYPE::DEBIT 
      ? - transaction.valor
      :   transaction.valor
      );

  if (-balance.saldo >= balance.limite) {
    res.status = 422;
    res.set_content("", "text/html");
    return;
  }

  auto response = database::createTransaction(connection.get(), clientId, transaction, balance);

  if(!response.has_value()) {
    res.set_content("", "text/html");
    std::cout << "[LOG:500]" << data << std::endl;
    res.status = 500;
    return;
  }

  nlohmann::json responseJson;
  responseJson["limite"] = balance.limite;
  responseJson["saldo"] = balance.saldo;

  res.status = 200;
  res.set_content(responseJson.dump(), "application/json");
}

void initDatabase() {
    namespace fs = std::filesystem;

    fs::remove("database.db"); 

    std::fstream s{"init.sql", s.in};

    if(!s.is_open()) {
        return;
    }

    auto connection = database::getConnection(false);

    std::stringstream sql;

    sql << s.rdbuf();

    database::run_stmt(connection.get(), sql.str().c_str());
}

int
main(int argc, char **argv) {
    initDatabase();

    // HTTP
    httplib::Server svr;

    svr.new_task_queue = [] { return new httplib::ThreadPool(1); };

    svr.Get(R"(/clientes/:id/extrato)", extract);
    svr.Post(R"(/clientes/:id/transacoes)", createTransaction);

    svr.listen("0.0.0.0", 9999);
    return 0;
}
