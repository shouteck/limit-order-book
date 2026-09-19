#include "lob/book.hpp"

namespace lob {

namespace {
[[noreturn]] void not_implemented(const char* what) {
    throw std::logic_error(std::string("Book::") + what +
                           " not implemented -- this is your part. See book.hpp");
}
} // namespace

void Book::add_impl(const Order&, const std::function<void(const Trade&)>&) {
    not_implemented("add");
}

bool Book::cancel(OrderId)              { not_implemented("cancel"); }
bool Book::modify(OrderId, Price, Quantity) { not_implemented("modify"); }

std::optional<Price> Book::best_bid() const { not_implemented("best_bid"); }
std::optional<Price> Book::best_ask() const { not_implemented("best_ask"); }
std::size_t          Book::order_count() const { not_implemented("order_count"); }
std::uint64_t        Book::resting_qty(Side) const { not_implemented("resting_qty"); }

} // namespace lob
