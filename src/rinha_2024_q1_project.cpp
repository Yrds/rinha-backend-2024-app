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

    //TODO stoi fix
    auto clientId = std::stoi(req.path_params.find("id"));

    if (clientId != req.path_params.end()) {
      res.status = 422;
      return;
    }

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

    res.set_content(data.dump(), "application/json");
    return;
}

void
createTransaction(const httplib::Request &req, httplib::Response &res) {
  nlohmann::json data = nlohmann::json::parse(req.body);

  std::cout << data << std::endl;

  auto clientId = std::stoi(req.path_params.at("id"));

  //auto result = database::getExtractByClientId(connection.get(), clientId);

  //if (!result.has_value()) {
  //  switch(result.error()) {

  //    default:
  //      res.status = result.error() == "Not found" ? 404 : 500;
  //      res.set_content(result.error(), "text/html");
  //  }

  //  return;
  //}

  /*flow
   * start transaction
   * verify if transaction.valor are greater than limite/saldo
   *  if not return 402 and end transaction
   *  if yes, proceed
   * if yes, then procede with a (database)transaction to lock table
   * return new balance(TransactionResponse)
   */

  std::basic_string<unsigned char> transactionType {reinterpret_cast<const unsigned char*>(
         data["tipo"].template get<std::string>().c_str()
         )};

  const models::Transaction transaction {
   data["valor"].template get<int>(),
   static_cast<models::TRANSACTION_TYPE>(transactionType.at(0)),
   data["descricao"].template get<std::string>()
  };

  auto connection = database::getConnection();


  auto response = database::createTransaction(connection.get(), clientId, transaction);

  if(response.has_value()) {
    const auto responseObject = *response;

    nlohmann::json responseJson;
    responseJson["limite"] = responseObject.limite;
    responseJson["saldo"] = responseObject.saldo;

    res.status = 200;
    res.set_content(responseJson.dump(), "application/json");
  } else {
    res.set_content(response.error(), "text/html");
    res.status = 500;
  }
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
    svr.Post(R"(/clientes/:id/transacoes)", createTransaction);

    svr.listen("0.0.0.0", 8080);
    return 0;
}
