// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <iostream>
#include <string>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <solv/solver.h>
#include <solv/transaction.h>

#include "solv-cpp/pool.hpp"
#include "solv-cpp/solver.hpp"
#include "solv-cpp/transaction.hpp"
#include "mamba/core/util_scope.hpp"

#include <boost/stacktrace.hpp>

template <>
struct fmt::formatter<boost::stacktrace::frame> : formatter<std::string_view>
{
    template <typename FORMAT_CONTEXT>
    auto format(const boost::stacktrace::frame& rhs, FORMAT_CONTEXT& ctx) const
    {
        return fmt::format_to(ctx.out(), "{}:{}\n", rhs.source_file(), rhs.source_line());
    }
};

template <>
struct fmt::formatter<boost::stacktrace::stacktrace> : formatter<std::string_view>
{
    template <typename FORMAT_CONTEXT>
    auto format(const boost::stacktrace::stacktrace& rhs, FORMAT_CONTEXT& ctx) const
    {
        return fmt::format_to(ctx.out(), "{}", fmt::join(rhs.as_vector(), "\n"));
    }
};


namespace solv
{

    template <typename F>
    struct on_scope_exit
    {
        F func;

        explicit on_scope_exit(F&& f)
            : func(std::forward<F>(f))
        {
        }

        ~on_scope_exit()
        {
            try
            {
                func();
            }
            catch (const std::exception& ex)
            {
                fmt::print("############ \n Scope exit error - ABORTING : {}\n{}", ex.what(), boost::stacktrace::stacktrace());
                __debugbreak();
                std::abort();
            }
            catch (...)
            {
                fmt::print("############ \n Scope exit unknown error - ABORTING :\n{}", boost::stacktrace::stacktrace());
                __debugbreak();
                std::abort();
            }
        }

        // Deactivate copy & move until we implement moves
        on_scope_exit(const on_scope_exit&) = delete;
        on_scope_exit& operator=(const on_scope_exit&) = delete;
        on_scope_exit(on_scope_exit&&) = delete;
        on_scope_exit& operator=(on_scope_exit&&) = delete;
    };

    void ObjTransaction::TransactionDeleter::operator()(::Transaction* ptr)
    {
        fmt::print("\nKLAIM DEBUG: ObjTransaction::TransactionDeleter::operator()({}) - BEGIN", fmt::ptr(ptr) );

        ::transaction_free(ptr);
        //std::memset(ptr, 0, sizeof(::Transaction));
        //new ((void*) ptr) // makes sure we are writing there but it's not undefined behavior because we allocate
        //    std::ptrdiff_t[sizeof(::Transaction)]{ 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF };
        const std::ptrdiff_t dead_memory[] = { 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF };
        std::memcpy(ptr, dead_memory, sizeof(::Transaction)); // might be UB?

        fmt::print("\nKLAIM DEBUG: ObjTransaction::TransactionDeleter::operator()({}) - END", fmt::ptr(ptr) );
    }

    ObjTransaction::ObjTransaction(const ObjPool& pool)
        : ObjTransaction(::transaction_create(const_cast<::Pool*>(pool.raw())))
    {
    }

    ObjTransaction::ObjTransaction(::Transaction* ptr) noexcept
        : m_transaction(ptr)
    {
        fmt::print("\nKLAIM: ObjTransaction::ObjTransaction({}) :  this = {}", fmt::ptr(ptr), fmt::ptr(this));
    }

    ObjTransaction::~ObjTransaction()
    {
        fmt::print("\nKLAIM: ObjTransaction::~ObjTransaction() with explicit transaction reset - BEGIN : this = {}", fmt::ptr(this));
        on_scope_exit _{ [&]{
            fmt::print("\nKLAIM: ObjTransaction::~ObjTransaction() with explicit transaction reset  - END : this = {}", fmt::ptr(this));
        } };
        m_transaction.reset();
        fmt::print("\nKLAIM: ObjTransaction::~ObjTransaction() with explicit transaction reset - overwriting ObjTransaction's memory : this = {}", fmt::ptr(this));
        std::memset(this, 0, sizeof(ObjTransaction));
    }

    ObjTransaction::ObjTransaction(const ObjTransaction& other)
        : ObjTransaction(::transaction_create_clone(const_cast<::Transaction*>(other.raw())))
    {
    }

    auto ObjTransaction::operator=(const ObjTransaction& other) -> ObjTransaction&
    {
        *this = ObjTransaction(other);
        return *this;
    }

    auto
    ObjTransaction::from_solvables(const ObjPool& pool, const ObjQueue& solvables) -> ObjTransaction
    {
        fmt::print("\nKLAIM: from_solvables(pool, solvables) : pool.raw() = {}, solvables.raw() = {}", fmt::ptr(pool.raw()), fmt::ptr(solvables.raw()));
        on_scope_exit _{ [&] {
            std::cerr <<fmt::format("\nKLAIM: from_solvables(pool, solvables) : pool.raw() = {}, solvables.raw() = {}", fmt::ptr(pool.raw()), fmt::ptr(solvables.raw()));
        } };

        return ObjTransaction{ ::transaction_create_decisionq(
            const_cast<::Pool*>(pool.raw()),
            const_cast<::Queue*>(solvables.raw()),
            nullptr
        ) };
    }

    namespace
    {
        void
        assert_same_pool([[maybe_unused]] const ObjPool& pool, [[maybe_unused]] const ObjTransaction& trans)
        {
            fmt::print("\nKLAIM: assert_same_pool(pool, solvables) : pool.raw() = {}, trans.raw() = {}", fmt::ptr(pool.raw()), fmt::ptr(trans.raw()));
            assert(pool.raw() == trans.raw()->pool);
        }
    }

    auto ObjTransaction::from_solver(const ObjPool& pool, const ObjSolver& solver) -> ObjTransaction
    {
        fmt::print("\nKLAIM: ObjTransaction::from_solver({}, solver) - BEGIN: pool.raw() = {}, solver.raw() = {}", fmt::ptr(std::addressof(pool)), fmt::ptr(pool.raw()), fmt::ptr(solver.raw()));

        on_scope_exit _{ [&] {
            std::cerr << "\nKLAIM: ObjTransaction::from_solver(...) - END" << std::endl;
        } };

        auto trans = ObjTransaction{ ::solver_create_transaction(const_cast<::Solver*>(solver.raw())) };
        assert_same_pool(pool, trans);
        return trans;
    }

    auto ObjTransaction::raw() -> ::Transaction*
    {
        return m_transaction.get();
    }

    auto ObjTransaction::raw() const -> const ::Transaction*
    {
        return m_transaction.get();
    }

    auto ObjTransaction::empty() const -> bool
    {
        return raw()->steps.count <= 0;
    }

    auto ObjTransaction::size() const -> std::size_t
    {
        assert(raw()->steps.count >= 0);
        return static_cast<std::size_t>(raw()->steps.count);
    }

    auto ObjTransaction::steps() const -> ObjQueue
    {
        ObjQueue out = {};
        for_each_step_id([&](auto id) { out.push_back(id); });
        return out;
    }

    auto ObjTransaction::step_type(const ObjPool& pool, SolvableId step, TransactionMode mode) const
        -> TransactionStepType
    {
        fmt::print("\nKLAIM: ObjTransaction::step_type(...) - BEGIN : this = {}", fmt::ptr(this));

        on_scope_exit _{ [&] {
            fmt::print("\nKLAIM: ObjTransaction::step_type(...) - END : this = {}", fmt::ptr(this));
        } };

        assert_same_pool(pool, *this);
        return ::transaction_type(const_cast<::Transaction*>(raw()), step, mode);
    }

    auto
    ObjTransaction::step_newer(const ObjPool& pool, SolvableId step) const -> std::optional<SolvableId>
    {
        fmt::print("\nKLAIM: ObjTransaction::step_newer(...) - BEGIN : this = {}", fmt::ptr(this));

        on_scope_exit _{ [&] {
            fmt::print("\nKLAIM: ObjTransaction::step_newer(...) - END : this = {}", fmt::ptr(this));
        } };

        assert_same_pool(pool, *this);
        if (const auto solvable = pool.get_solvable(step); solvable && solvable->installed())
        {
            if (auto id = ::transaction_obs_pkg(const_cast<::Transaction*>(raw()), step); id != 0)
            {
                return { id };
            }
        }
        return std::nullopt;
    }

    auto ObjTransaction::step_olders(const ObjPool& pool, SolvableId step) const -> ObjQueue
    {
        fmt::print("\nKLAIM: ObjTransaction::step_olders(...) - BEGIN : this = {}", fmt::ptr(this));

        on_scope_exit _{ [&] {
            fmt::print("\nKLAIM: ObjTransaction::step_olders(...) - END : this = {}", fmt::ptr(this));
        } };

        assert_same_pool(pool, *this);
        auto out = ObjQueue{};
        if (const auto solvable = pool.get_solvable(step); solvable && !solvable->installed())
        {
            ::transaction_all_obs_pkgs(const_cast<::Transaction*>(raw()), step, out.raw());
        }

        return out;
    }

    void ObjTransaction::order(const ObjPool& pool, TransactionOrderFlag flag)
    {
        fmt::print("\nKLAIM: ObjTransaction::order({}, {}) - BEGIN : this = {}", fmt::ptr(std::addressof(pool)), flag, fmt::ptr(this));
        on_scope_exit{ [&]{
            fmt::print("\nKLAIM: ObjTransaction::order({}, {}) - END (SKIPPED) : this = {}", fmt::ptr(std::addressof(pool)), flag, fmt::ptr(this));
        }};
        assert_same_pool(pool, *this);
        fmt::print("\nKLAIM: calling transaction_order now : this = {}", fmt::ptr(std::addressof(pool)), flag, fmt::ptr(this));
        ::transaction_order(raw(), flag);
    }

    auto ObjTransaction::classify(const ObjPool& pool, TransactionMode mode) const -> ObjQueue
    {
        fmt::print("\nKLAIM: ObjTransaction::classify({}, {}) - BEGIN : this = {}", fmt::ptr(std::addressof(pool)), mode, fmt::ptr(this));
        on_scope_exit{ [&]{
            fmt::print("\nKLAIM: ObjTransaction::classify({}, {}) - END (SKIPPED) : this = {}", fmt::ptr(std::addressof(pool)), mode, fmt::ptr(this));
        }};
        assert_same_pool(pool, *this);
        auto out = ObjQueue{};
        fmt::print("\nKLAIM: ObjTransaction::classify({}, {}) - calling transaction_classify now : this = {}", fmt::ptr(std::addressof(pool)), mode, fmt::ptr(this));
        ::transaction_classify(const_cast<::Transaction*>(raw()), mode, out.raw());
        return out;
    }

    auto ObjTransaction::classify_pkgs(
        const ObjPool& pool,
        TransactionStepType type,
        StringId from,
        StringId to,
        TransactionMode mode
    ) const -> ObjQueue
    {
        fmt::print("\nKLAIM: ObjTransaction::classify_pkgs(...) - BEGIN : this = {}", fmt::ptr(this));

        on_scope_exit _{ [&] {
            fmt::print("\nKLAIM: ObjTransaction::classify_pkgs(...) - END : this = {}", fmt::ptr(this));
        } };
        assert_same_pool(pool, *this);
        auto out = ObjQueue{};
        fmt::print("\nKLAIM: ObjTransaction::classify_pkgs(...) - calling transaction_classify_pkgs : this = {}", fmt::ptr(this));
        ::transaction_classify_pkgs(const_cast<::Transaction*>(raw()), mode, type, from, to, out.raw());
        return out;
    }

}
