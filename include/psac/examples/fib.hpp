#ifndef PSAC_EXAMPLES_FIBONACCI_HPP_
#define PSAC_EXAMPLES_FIBONACCI_HPP_

#include "psac/types.hpp"
#include <mutex>
#include <psac/psac.hpp>

extern std::mutex cout_mutex;

#define LOG_WORKER(TEXT)                                                       \
  {                                                                            \
    std::lock_guard lock(cout_mutex);                                          \
    std::cout << "[WORKER]: " << TEXT << '\n';                                 \
  }

psac_function(fib, psac::Mod<int> *n_mod, psac::Mod<int> *result) {
  psac_read((int n), (n_mod), {
    LOG_WORKER("Entering n = " << n);

    if (n < 2) {
      LOG_WORKER("Base case for = " << n);
      psac_write(result, n);
    } else {
      auto n1_mod = psac_alloc(int);
      auto n2_mod = psac_alloc(int);

      psac_write(n1_mod, n - 1);
      psac_write(n2_mod, n - 2);
      LOG_WORKER("Split (" << n << "->(" << n - 1 << ", " << n - 2 << "))");

      auto res1 = psac_alloc(int);
      auto res2 = psac_alloc(int);
      psac_par(
          { psac_call(fib, n1_mod, res1); }, { psac_call(fib, n2_mod, res2);
          });

      psac_read((int r1, int r2), (res1, res2), {
        // LOG_WORKER("Sum for n=" << n << ": fib(" << n - 1 << ")=" << r1
        //                         << " + fib(" << n - 2 << ")=" << r2 << " =
        // "
                                // << r1 + r2);
        psac_write(result, r1 + r2);
      });
    }
  });
}

// psac_function(fib_internal, int n, psac::Mod<int> *result) {
//   LOG_WORKER("Entering n = " << n);

//   if (n < 2) {
//     LOG_WORKER("Base case for = " << n);
//     psac_write(result, n);
//   } else {
//     LOG_WORKER("Split (" << n << "->(" << n - 1 << ", " << n - 2 << "))");

//     auto res1 = psac_alloc(int);
//     auto res2 = psac_alloc(int);
//     psac_par(
//         { psac_call(fib_internal, n - 1, res1); },
//         { psac_call(fib_internal, n - 2, res2); });

//     psac_read((int r1, int r2), (res1, res2), {
//       // LOG_WORKER("Sum for n=" << n << ": fib(" << n - 1 << ")=" << r1
//       //                         << " + fib(" << n - 2 << ")=" << r2 << " = "
//       //                         << r1 + r2);
//       psac_write(result, r1 + r2);
//     });
//   }
// }

// psac_function(fib, psac::Mod<int> *n_mod, psac::Mod<int> *result) {
//   psac_read((int n), (n_mod), {
//     LOG_WORKER("N changed to: " << n);
//     psac_call(fib_internal, n, result);
//   });
// }

#endif
