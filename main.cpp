#include <scheduler/scheduler.h>
#include <iostream>
#include <chrono>
#include <random>

int main() {
    std::cout << "main thread: " << std::this_thread::get_id() << '\n';

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, 10);
    double average, minimum, maximum;
    uint64_t missed;

    // generate a random number
    int r = dist(gen);
    auto executions = std::make_shared<std::atomic<uint64_t>>(0);
    {
        scheduler::Scheduler scheduler{4};

        auto job = [executions](){ 
            executions->fetch_add(1);
            std::cout << executions->load() << ". I am called by thread " << std::this_thread::get_id() << '\n';
        };

        scheduler.scheduleRecurring(job, dist(gen), std::chrono::milliseconds{1});

        scheduler.schedule(job, dist(gen));
        scheduler.schedule(job, dist(gen));

        for (int i = 0; i < 100; ++i) {
            scheduler.schedule(job, 0, std::chrono::steady_clock::now() + std::chrono::milliseconds{1});
        }

        for (int i = 0; i < 100; ++i) {
            int prio = dist(gen);
            scheduler.schedule(
                [executions, i]() {
                    executions->fetch_add(1, std::memory_order_relaxed);
                    std::cout << "task " << i
                            << " ran (count="
                            << executions->load() 
                            << ") on thread "
                            << std::this_thread::get_id()
                            << "\n";
                },
                0,
                std::chrono::steady_clock::now() + std::chrono::milliseconds{1}
            );
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{1000});
        auto [avg, min, max] = scheduler.getLatencyStatistics();
        missed = scheduler.getMissedTasks();
        average = avg;
        minimum = min;
        maximum = max;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    std::cout   << "Results: \n"
                << "Avg: " << (average / 1e6) << " ms\n"
                << "Min: " << (minimum / 1e6) << " ms\n"
                << "Max: " << (maximum / 1e6) << " ms\n"
                << "Missed tasks: " << missed << "\n";
    return 0;
}