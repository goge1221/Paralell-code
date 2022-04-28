#include <iostream>
#include <thread>
#include <random>
#include <chrono>
#include <complex>
#include <sstream>
#include <string>
#include <fstream>
#include <iomanip>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>

#include "a1-helpers.hpp"

template <typename T>
class SafeQ
{
private:
    std::queue<T> q; // no other data structures are allowed
    // extend as needed
    std::mutex m;

public:
    void push(T value)
    {
        // synchronization needed?
        //std::lock_guard<std::mutex>guard(m);
        q.push(value);
    }

    void pop(T &value)
    {
        // todo:
        // in a thread-safe way take the front element
        // and pop it from the queue
        // multiple consumers may be accessing this method
        // if not empty, remove the element from the queue
        // std::lock_guard<std::mutex>guard(m);
        if (!q.empty())
        {
            value = q.front();
            q.pop();
        }

    }

    std::shared_ptr<T> wait_and_pop()
    {
        // todo:
        // in a thread-safe way take the front element
        // and pop it from the queue
        // multiple consumers may be accessing this method
        std::lock_guard<std::mutex>guard(m);
        if ( !q.empty() ){
            std::shared_ptr<T> res_ptr (std::make_shared<T>(q.front()));
            q.pop();
            return res_ptr;
        }
        return nullptr;
    }

    size_t size()
    {
        // synchronization needed?
        std::lock_guard<std::mutex>guard(m);
        return q.size();
    }

    bool empty()
    {
        // synchronization needed?
        std::lock_guard<std::mutex>guard(m);
        return q.empty();
    }
};

/**
 * To be executed by the master thread
 * The function reads numbers from the file
 * and puts them into the given queue
 *
 * @param[in] filename
 * @param[inout] q
 * @returns The number of produced items
 *
*/
int producer(std::string filename, SafeQ<int> &q)
{
    int produced_count = 0;

    // while there are entries in the file
    // put numbers into the queue "q"
    std::ifstream ifs(filename);

    while (!ifs.eof()) {
        int num;
        ifs >> num;
        q.push(num);
        produced_count++;
    }

    ifs.close();

    return produced_count;
}

/**
 * To be executed by worker threads
 * The function removes a number from the queue "q"
 * and does the processing
 * Implement 2 versions with atomic and with mutexes
 * extend as needed
 *
 * @param[inout] q
 * @param[inout] primes
 * @param[inout] nonprimes
 * @param[inout] mean
 * @param[inout] number_counts
 *
*/

void worker(SafeQ<int> &q, std::atomic<int> &primes, std::atomic<int> &nonprimes, std::atomic<double> &sum, std::atomic<int> &consumed_count, std::vector<std::atomic<int>> &number_counts_atomic)
{
    // implement: use synchronization
    // Note: This part may need some rearranging and rewriting
    // the while loop cannot just check for the size,
    // it has to now wait until the next element can be popped,
    // or it has to terminate if producer has finished and the queue is empty.
    // This could be fully or partially implemented in the SafeQ

    auto a = q.wait_and_pop();

    while (a != nullptr)
    {
        int num = *a;
        consumed_count++;
        if (kernel(num) == 1) {
            primes++;
        } else {
            nonprimes++;
        }
        sum.fetch_add(num);
        number_counts_atomic[num%10]++;
        a = q.wait_and_pop();
    }

}

int main(int argc, char **argv)
{
    int num_threads = std::thread::hardware_concurrency(); // you can change this default to std::thread::hardware_concurrency()
    bool no_exec_times = false, only_exec_times = false;; // reporting of time measurements
    std::string filename = "input.txt";
    parse_args(argc, argv, num_threads, filename, no_exec_times, only_exec_times);

    // The actuall code
    std::atomic<int> primes{0}, nonprimes{0}, count{0};
    std::atomic<int> consumed_count{0};
    double mean = 0.0;
    std::atomic<double> sum{0.0};
    // vector for storing numbers ending with different digits (0-9)
    std::vector<int> number_counts(10, 0);

    std::vector<std::atomic<int>>number_counts_atomic(10);

    for(int i = 0; i < number_counts_atomic.size(); ++i){
        number_counts_atomic.at(i).load(static_cast<std::memory_order>(0));
    }


    // Queue that needs to be made safe
    // In the simple form it takes integers
    SafeQ<int> q;

    // put you worker threads here
    std::vector<std::thread> workers;

    // time measurement
    auto t1 =  std::chrono::high_resolution_clock::now();

    // implement: call the producer function with futures/async
    auto future = std::async(producer,filename, std::ref(q));
    int produced_count = future.get();

    // implement: spawn worker threads - transform to spawn num_threads threads and store in the "workers" vector
    for (int i=0;i<num_threads;++i) {
        workers.emplace_back(worker, std::ref(q), std::ref(primes), std::ref(nonprimes), std::ref(sum), std::ref(consumed_count), std::ref(number_counts_atomic));
    }

    for(auto &a : workers) a.join();

    for(int i = 0; i < number_counts_atomic.size(); ++i){
        number_counts.at(i) = static_cast<int>(number_counts_atomic.at(i));
    }

    mean = sum/consumed_count;

    // end time measurement
    auto t2 =  std::chrono::high_resolution_clock::now();

    // do not remove
    if ( produced_count != consumed_count ) {

        std::cout << "[error]: produced_count (" << produced_count << ") != consumed_count (" << consumed_count << ")." <<  std::endl;
    }

    // priting the results
    print_output(num_threads, primes, nonprimes, mean, number_counts, t1, t2, only_exec_times, no_exec_times);

    return 0;
}
