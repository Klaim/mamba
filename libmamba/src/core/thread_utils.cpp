// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <atomic>
#ifndef _WIN32
#include <signal.h>
#endif

#include "mamba/core/invoke.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/thread_utils.hpp"

namespace mamba
{
    /***********************
     * thread interruption *
     ***********************/

    namespace
    {
        std::atomic<bool> sig_interrupted(false);
    }

    void set_default_signal_handler()
    {
        std::signal(SIGINT, [](int /*signum*/) { set_sig_interrupted(); });
    }

    bool is_sig_interrupted() noexcept
    {
        return sig_interrupted.load();
    }

    void set_sig_interrupted() noexcept
    {
        sig_interrupted.store(true);
    }

    void interruption_point()
    {
        if (is_sig_interrupted())
        {
            throw thread_interrupted();
        }
    }

    /*******************************
     * thread count implementation *
     *******************************/

    namespace
    {
        int thread_count = 0;
        std::mutex clean_mutex;
        std::condition_variable clean_var;

        std::mutex main_mutex;
        std::condition_variable main_var;
    }  // namespace

    void increase_thread_count()
    {
        std::unique_lock<std::mutex> lk(clean_mutex);
        ++thread_count;
    }

    void decrease_thread_count()
    {
        std::unique_lock<std::mutex> lk(clean_mutex);
        --thread_count;
        std::notify_all_at_thread_exit(clean_var, std::move(lk));
    }

    int get_thread_count()
    {
        return thread_count;
    }

    void wait_for_all_threads()
    {
        std::unique_lock<std::mutex> lk(clean_mutex);
        clean_var.wait(lk, []() { return thread_count == 0; });
    }

    /*************************
     * thread implementation *
     *************************/

    bool thread::joinable() const noexcept
    {
        return m_thread.joinable();
    }

    std::thread::id thread::get_id() const noexcept
    {
        return m_thread.get_id();
    }

    void thread::join()
    {
        m_thread.join();
    }

    void thread::detach()
    {
        m_thread.detach();
    }

    std::thread::native_handle_type thread::native_handle()
    {
        return m_thread.native_handle();
    }

    /**********************
     * interruption_guard *
     **********************/

    std::function<void()> interruption_guard::m_cleanup_function;

    interruption_guard::~interruption_guard()
    {
        wait_for_all_threads();
        if (is_sig_interrupted() || std::uncaught_exceptions() > 0)
        {
            const auto result = safe_invoke(std::move(m_cleanup_function));
            if (!result)
            {
                LOG_ERROR << "interruption_guard invocation failed: " << result.error().what();
            }
        }
    }

}  // namespace mamba
