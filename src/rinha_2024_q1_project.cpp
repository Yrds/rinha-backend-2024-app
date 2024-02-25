#include <iostream>
#include <fstream>
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "database.hpp"

#define PROJECT_NAME "rinha-2024-q1-project"

void
extract(const httplib::Request &, httplib::Response &res) {
    auto connection = database::getConnection();

    auto data = nlohmann::json({
        {"test", 10}
        });

    res.set_content(data.dump(4), "application/json");
}


void initDatabase() {

    std::fstream s{"init.sql", s.binary | s.trunc | s.in | s.out};

    if(!s.is_open()) {
        return;
    }

    auto connection = database::getConnection();

    std::string sql;

    s >> sql;

    database::run_stmt(connection.get(), sql.c_str());
}

int
main(int argc, char **argv) {

    initDatabase();
    // HTTP
    httplib::Server svr;

    svr.Get(R"(/clientes/(\d+)/extrato)", extract);

    svr.listen("0.0.0.0", 8080);
    return 0;
}
