#include <memory>
#include <expected>

#include "models.hpp"
#include "unexpected_codes.hpp"

namespace database {

struct Connection;

//Database specific
std::unique_ptr<Connection, void(*)(Connection*)> getConnection(bool transactional = false);
void run_stmt(Connection*, const char *);

//Model operations

std::expected<models::Extract, UNEXPECTED_CODE>
getExtractByClientId(Connection* connection, int clientId);

std::expected<std::vector<models::TransactionHistory>, UNEXPECTED_CODE>
getLastTransactionsByClientId(Connection* connection, int clientId);

std::expected<models::TransactionResponse, UNEXPECTED_CODE>
createTransaction(Connection* connection, const int clientId,
                  const models::Transaction &transaction, models::TransactionResponse& balance);

std::expected<models::TransactionResponse, UNEXPECTED_CODE>
getBalance(Connection* connection, const int clientId);

std::optional<UNEXPECTED_CODE>
bind(Connection* connection, std::string_view value);

std::optional<UNEXPECTED_CODE>
bind(Connection* connection, int value);

//std::optional<UNEXPECTED_CODE>
//bind(Connection* connection, std::chrono::time_point& value);
}
