#ifndef PSAC_EXAMPLES_SUM_HPP_
#define PSAC_EXAMPLES_SUM_HPP_

#include <mutex>
#include <psac/psac.hpp>

extern std::mutex cout_mutex;

#define LOG_WORKER(TEXT)                                                       \
  {                                                                            \
    std::lock_guard lock(cout_mutex);                                          \
    std::cout << "[WORKER]: " << TEXT << '\n';                                 \
  }

// Registers `sum` in the computation graph.
template <typename It>
psac_function(sum, It inputs_begin, int left, int right,
              psac::Mod<int> *result) {

  LOG_WORKER("(SPLIT) in indexes [" << left << ", " << right << "]");

  if (left == right - 1) {
    // psac_read mówi, że zależy od wartości tego Moda.
    // (int val) - deklarujemy zmienną lokalną, do której zostanie wpisana
    // aktualna wartość. (inputs_begin + lo) - wskazujemy, który mod czytamy.
    psac_read((int val), (inputs_begin + left), {
      LOG_WORKER("(LEAF) for index " << left << " value " << val);
      // Zapisuje wynik do Moda `result`.
      // Teraz zapamiętuje się, ze ten `result powstał z tego konkretnego `val`
      //   psac_write(result, val * val);
      psac_write(result, val);
    })
  } else {
    int mid = left + (right - left) / 2;
    // Alokacja tymczasowych Modifiable wewnątrz grafu do których będziemy
    // zapisywać wyniki.
    auto left_res = psac_alloc(int);
    auto right_res = psac_alloc(int);

    // Odpala dwa bloki równolegle.
    psac_par({psac_call(sum, inputs_begin, left, mid, left_res)},
             {psac_call(sum, inputs_begin, mid, right, right_res)});

    psac_read((int l, int r), (left_res, right_res), {
      LOG_WORKER("(SUM) " << l << " + " << r);
      // Czekamy na oba wyniki z obu gałęzi. Gdy oba będa gotowe to psac wrzuci
      // je do `l` i `r`. Zapisujemy do modifiable na wynik.
      psac_write(result, l + r);
    });
  }
}

#endif