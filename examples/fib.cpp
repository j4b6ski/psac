#include "psac/examples/fib.hpp"
#include <iostream>
#include <psac/psac.hpp>

std::mutex cout_mutex;

#define LOG(TEXT)                                                              \
  {                                                                            \
    std::lock_guard lock(cout_mutex);                                          \
    std::cout << "[PSAC_FIB]: " << TEXT << '\n';                               \
  }

int main() {
  LOG("======================================");
  LOG("Start Init");

  psac::Mod<int> n;
  psac::Mod<int> result;

  LOG("Run PSAC for 10");

  psac_write(&n, 3);
  auto comp = psac_run(fib, &n, &result);
  LOG("Result of fib(10) is: " << result.value);

  LOG("Change n from 10 to 11");
  psac_write(&n, 4);

  LOG("Propagating after change");
  psac_propagate(comp);

  LOG("Result of sum after propagation is: " << result.value);

  psac::GarbageCollector::run();
  LOG("======================================");
}
