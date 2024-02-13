#include <iostream>
#include "httplib.h"
#include "nlohmann/json.hpp"

#define PROJECT_NAME "rinha-2024-q1-project"

void
extract(const httplib::Request &, httplib::Response &res) {
    auto data = nlohmann::json({
        {"test", 10}
        });

    std::cout << "oi" << std::endl;
    res.set_content(data.dump(4), "application/json");
}

int
main(int argc, char **argv) {
    // HTTP
    httplib::Server svr;

    svr.Get(R"(/clientes/(\d+)/extrato)", extract);

    svr.listen("0.0.0.0", 8080);
    return 0;
}
