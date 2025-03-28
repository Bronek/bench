#include "threadpool.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <random>
#include <unordered_set>


constexpr int workerThreads = 5;
constexpr int inputSize = 3;


static_assert(inputSize <= 8); // Map every input to one of 8 functions below
void f0(int &t, int i) { t += i + 12; }
void f1(int &t, int i) { t += i * 2; }
void f2(int &t, int i) { t += i / 2; }
void f3(int &t, int i) { t += i * 5; }
void f4(int &t, int i) { t += i / 5; }
void f5(int &t, int i) { t += i - 40; }
void f6(int &t, int i) { t += i % 12; }
void f7(int &t, int i) { t += i * 12; }
using ft = void (*)(int &, int);

struct accumulator {
  std::atomic<std::int64_t> count = 0;
  std::atomic<std::int64_t> total = 0;

  std::chrono::steady_clock::time_point start() { return std::chrono::steady_clock::now(); }
  void stop(std::chrono::steady_clock::time_point start)
  {
    auto const time = std::chrono::steady_clock::now() - start;
    count += 1;
    total += time.count();
  }

  inline friend std::ostream & operator<<(std::ostream &o, accumulator const &self)
  {
    auto const c = self.count.load();
    auto const t = self.total.load();
    if (c == 0)
      return (o << "N/A");

    return (o << t / c << " (" << c << ')');
  }
};

void benchMap(int &dummy, int input, accumulator &acc)
{
  static std::map<int, ft> const fields{
      {0, &f0}, {1, &f1}, {2, &f2}, {3, &f3}, {4, &f4}, {5, &f5}, {6, &f6}, {7, &f7},
  };

  auto const start = acc.start();
  auto const p = fields.find(input);
  (*(p->second))(dummy, input);
  acc.stop(start);
}

void benchArray(int &dummy, int input, accumulator &acc)
{
  static std::array<ft, 8> fields = {&f0, &f1, &f2, &f3, &f4, &f5, &f6, &f7};

  auto const start = acc.start();
  auto const p = fields[input];
  (*p)(dummy, input);
  acc.stop(start);
}

void benchSwitch(int &dummy, int input, accumulator &acc)
{
  auto const start = acc.start();
  switch (input) {
  case 0:
    f0(dummy, input);
    break;
  case 1:
    f1(dummy, input);
    break;
  case 2:
    f2(dummy, input);
    break;
  case 3:
    f3(dummy, input);
    break;
  case 4:
    f4(dummy, input);
    break;
  case 5:
    f5(dummy, input);
    break;
  case 6:
    f6(dummy, input);
    break;
  case 7:
    f7(dummy, input);
    break;
  }
  acc.stop(start);
}

int main()
{
  std::atomic<int> i = 0;
  std::atomic<std::size_t> k = 0;
  accumulator accMap = {};
  accumulator accArray = {};
  accumulator accSwitch = {};

  // return 0;

  // Used by pseudo-realistic background task
  std::random_device dev;

  {
    threadpool pool(workerThreads);

    for (int j = 1; j < 100'000; ++j) {
      pool.submit([&] {
        std::ranlux48 rnd;
        rnd.seed(dev());
        std::uniform_int_distribution<int> dist(0, 1000);
        int dummy = 0;

        auto const l = (i += 1);
        if ((l % 100) == 0) {
          // Actual benchmarks
          if ((l % 300) == 0) {
            benchMap(dummy, dist(rnd) % inputSize, accMap);
          } else if (((l + 100) % 300) == 0) {
            benchArray(dummy, dist(rnd) % inputSize, accArray);
          } else {
            benchSwitch(dummy, dist(rnd) % inputSize, accSwitch);
          }

          k = dummy;
          return;
        }

        // Pseudo-realistic background task
        std::unordered_set<int> set;
        while (true) {
          auto const m = dist(rnd);
          if (m < 5)
            break;
          set.insert(m);
        }
        if (set.empty())
          return;

        int total = 0;
        for (auto const element : set)
          total += element;

        k += (total / set.size());
      });
    }

    pool.stop();
  }

  std::cout << "map: " << accMap << '\n' << "array: " << accArray << '\n' << "switch: " << accSwitch << std::endl;
  return (((int)k * 10) % 5);
}
