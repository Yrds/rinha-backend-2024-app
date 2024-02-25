#include <memory>
#include <string_view>

#include "models.hpp"

namespace database {

struct Connection;

//Database specific
std::unique_ptr<Connection, void(*)(Connection*)> getConnection();
void run_stmt(Connection*, const char *);

//Model operations
models::Extract getExtractByClientId(Connection* connection, int clientId);

}
