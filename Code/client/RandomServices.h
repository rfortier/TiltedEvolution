#pragma once

#include <random>
#include <chrono>

// Random class. Those who think the original rand() functions are good 
// enough have never been burned by how not-random they are (especially in
// small ranges). But the newer std:: capabilities are tediously wordy
// and hard to get right inline. So, an interface.
//
// Adding the wordiness of std::chrono is even worse, so provide
// random durations in addition to random ranges.
//
class RandomServices
{
  public: 
    // Singleton
    RandomServices(const RandomServices&) = delete;
    RandomServices(RandomServices&&) = delete;
    RandomServices& operator=(const RandomServices&) = delete;
    RandomServices& operator=(RandomServices&&) = delete;
    static RandomServices& Get();

    // Methods
    uint64_t randomRange(const uint64_t acMin, const uint64_t acMax);

    using clock = std::chrono::steady_clock;
    clock::duration          RandomizedDuration(const clock::duration acMin = 3s, const clock::duration acMax = 10s);
    clock::time_point        RandomizedDeadline(const clock::duration acMin = 5s, const clock::duration acMax = 6s);
    static clock::time_point& LatestDeadline() noexcept
    {
        return RandomServices::Get().m_latestDeadline;
    }

    std::mt19937_64 m_randomEngine;

  private:   
    clock::time_point m_latestDeadline;
    RandomServices()
    {
        std::mt19937_64 m_randomEngine{std::default_random_engine{}()};
        m_latestDeadline = clock::now();
    };
};

