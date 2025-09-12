module;

#include <atomic>

export module kernel.spin;

import arch;
import lib.type_utility;

export namespace kernel::sync
{
    class spinlock_irq_t : lib::no_copy_move
    {
    private:
        std::atomic_flag flag_;

    public:
        spinlock_irq_t(void) noexcept
            : flag_{}
        {
        }

        void lock(void) noexcept
        {
            while (flag_.test_and_set(std::memory_order_seq_cst))
                ;
        }

        bool try_lock(void) noexcept
        {
            return !flag_.test_and_set(std::memory_order_seq_cst);
        }

        void unlock(void) noexcept
        {
            flag_.clear(std::memory_order_seq_cst);
        }
    };

    class spinlock_t : lib::no_copy_move
    {
    private:
        spinlock_irq_t raw_lock_;
        bool is_intr_on_;

        using task = arch::arch_traits::task;

    public:
        spinlock_t(void) noexcept
            : raw_lock_{}, is_intr_on_{true}
        {
        }

        void lock(void) noexcept
        {
            bool interrupt = task::is_interrupt_on();
            task::interrupt_off();

            raw_lock_.lock();

            is_intr_on_ = interrupt;
        }

        bool try_lock(void) noexcept
        {
            bool interrupt = task::is_interrupt_on();
            task::interrupt_off();

            bool ret = raw_lock_.try_lock();

            if (ret)
                is_intr_on_ = interrupt;
            else if (interrupt)
                task::interrupt_on();

            return ret;
        }

        void unlock(void) noexcept
        {
            bool interrupt = is_intr_on_;

            raw_lock_.unlock();

            if (interrupt)
                task::interrupt_on();
        }
    };
}