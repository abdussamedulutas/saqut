#ifndef SAQUT_DAP_SERVER
#define SAQUT_DAP_SERVER

#include "dap/dap_handler.hpp"
#include <iostream>

class DapServer {
public:
    void run();
private:
    DapHandler handler_{std::cout};
};

#endif // SAQUT_DAP_SERVER
