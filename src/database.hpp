#include <memory>
#include <expected>

#include "models.hpp"

namespace database {

struct Connection;

//Database specific
std::unique_ptr<Connection, void(*)(Connection*)> getConnection();
void run_stmt(Connection*, const char *);

//Model operations

std::expected<models::Extract, std::string>
  getExtractByClientId(Connection* connection, int clientId);

std::expected<std::vector<models::TransactionHistory>, std::string>
  getLastTransactionsByClientId(Connection* connection, int clientId);

std::string
  createTransaction(Connection* connection, const int clientId, const models::Transaction& transaction, const models::TransactionResponse& transactionResponse);

std::expected<models::TransactionResponse, std::string>
  getBalance(Connection* connection, int clientId);

}
