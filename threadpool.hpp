#ifndef THREADPOOL
#define THREADPOOL

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

struct threadpool {
  threadpool(std::size_t n)
  {
    for (std::size_t i = 0; i < n; ++i) {
      threads.emplace_back(&threadpool::run, this);
    }
  }

  ~threadpool()
  {
    stop();
    join();
  }

  void stop()
  {
    {
      std::unique_lock g(mutex);
      if (stopped)
        return;
      stopped = true;
      cv.notify_all();
    }
  }

  void join()
  {
    for (auto &t : threads) {
      t.join();
    }
    threads.clear();
  }

  bool submit(auto &&f)
  {
    std::lock_guard g(mutex);
    if (stopped)
      return false;

    queue.emplace(std::move(f));
    cv.notify_one();
    return true;
  }

private:
  void run()
  {
    while (true) {
      std::unique_lock g(mutex);
      cv.wait(g, [&, this] { return !queue.empty() || stopped; });
      if (stopped && queue.empty())
        return;

      if (queue.empty())
        continue;

      auto t = std::move(queue.front());
      queue.pop();
      g.unlock();

      t();
    }
  }

  using task = std::move_only_function<void()>;

  std::vector<std::thread> threads;
  std::queue<task> queue;
  bool stopped = false;
  std::mutex mutex;
  std::condition_variable cv;
};

#endif // THREADPOOL
