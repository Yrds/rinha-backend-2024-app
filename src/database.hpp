#include <memory>
#include <expected>

#include "models.hpp"
#include "unexpected_codes.hpp"

namespace database {

struct Connection;

//Database specific
std::unique_ptr<Connection, void(*)(Connection*)> getConnection();
void run_stmt(Connection*, const char *);

//Model operations

std::expected<models::Extract, UNEXPECTED_CODE>
  getExtractByClientId(Connection* connection, int clientId);

std::expected<std::vector<models::TransactionHistory>, UNEXPECTED_CODE>
  getLastTransactionsByClientId(Connection* connection, int clientId);

std::expected<models::TransactionResponse, std::string>
createTransaction(Connection* connection, const int clientId, const models::Transaction& transaction);

}
