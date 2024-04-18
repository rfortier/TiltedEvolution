#include "RandomServices.h"



RandomServices& RandomServices::Get()
{
    // The only instance. Guaranteed lazy initiated
    // Guaranteed that it will be destroyed correctly
    static RandomServices instance;

    return instance;
}

uint64_t RandomServices::randomRange(const uint64_t acMin, const uint64_t acMax)
{

    std::uniform_int_distribution<uint64_t> randomRange{acMin, acMax};
    return  randomRange(m_randomEngine);
}

RandomServices::clock::duration RandomServices::RandomizedDuration(
    const RandomServices::clock::duration acMin,
    const RandomServices::clock::duration acMax)
{
    auto duration = randomRange(acMin.count(), acMax.count());
    return std::chrono::duration<std::chrono::steady_clock::rep, std::chrono::steady_clock::period>(duration);
}

RandomServices::clock::time_point RandomServices::RandomizedDeadline(
    const RandomServices::clock::duration acMin,
    const RandomServices::clock::duration acMax)
{
    auto retval = clock::now() + RandomizedDuration(acMin, acMax);
    if (retval > m_latestDeadline)
        m_latestDeadline = retval;
    return retval;
}
