#include <string>
#include <chrono>
#include <vector>

namespace models {
  enum TRANSACTION_TYPE {
    DEBIT = 'd',
    CREDIT = 'c'
  };

  struct Transaction {
    int
      valor;
    TRANSACTION_TYPE
      tipo;
    std::string
      description;
  };

  struct Balance {
    int
      total;
    int
      limite;
  };

  struct BalanceHistory: public Balance {
    std::chrono::time_point<std::chrono::system_clock>
      data_extrato;
  };

  struct TransactionHistory: public Transaction {
    std::chrono::time_point<std::chrono::system_clock>
      realizada_em;
  };

  struct Extract {
    BalanceHistory saldo;
    std::vector<TransactionHistory> ultimas_transacoes;
  };
}
