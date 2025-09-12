export module contract.lock;

export namespace kernel::contract
{
    template <typename L>
    concept Lock = requires(L lock) {
        { lock.lock() } noexcept;
        { lock.try_lock() } noexcept;
        { lock.unlock() } noexcept;
    };
}