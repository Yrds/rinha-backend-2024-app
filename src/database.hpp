#include <memory>
#include "models.hpp"

namespace database {

struct Connection;

std::unique_ptr<Connection, void(*)(Connection*)> getConnection();



}

