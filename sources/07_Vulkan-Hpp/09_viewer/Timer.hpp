#pragma once

#include <chrono>
#include <iostream>
#include <string>

class Timer
{
public:
    explicit Timer(const std::string& info = {}) noexcept
        : m_info(info)
        , m_start(std::chrono::steady_clock::now())
    {
    }

    ~Timer() noexcept
    {
        auto consume    = std::chrono::steady_clock::now() - m_start;
        auto millisends = std::chrono::duration_cast<std::chrono::milliseconds>(consume);

        std::cout << m_info << " : " << millisends.count() << " ms" << std::endl;
    }

private:
    std::string m_info {};
    std::chrono::steady_clock::time_point m_start {};
};
