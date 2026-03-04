#include <iostream>
#include <psac/examples/sum.hpp>
#include <psac/psac.hpp>
#include <vector>

std::mutex cout_mutex;

#define LOG(TEXT)                                                              \
  {                                                                            \
    std::lock_guard lock(cout_mutex);                                          \
    std::cout << "[PSAC_SUM]: " << TEXT << '\n';                               \
  }

int main() {
  LOG("======================================");
  const int N = 4;

  LOG("Start Init");
  std::vector<psac::Mod<int>> inputs(N);
  psac::Mod<int> output;
  for (int i{0}; i < N; i++) {
    psac_write(&inputs[i], i + 1);
  }

  LOG("Run PSAC");

  // `comp` to pamięć do wykonanych obliczeń
  auto comp = psac_run(sum, inputs.begin(), 0, N, &output);
  LOG("Result of sum is: " << output.value);

  LOG("Change input[0] from 1 to 10");
  psac_write(&inputs[0], 10);

  LOG("Propagating after change");
  psac_propagate(comp);

  LOG("Result of sum after propagation is: " << output.value);

  psac::GarbageCollector::run();
  LOG("======================================");
}
