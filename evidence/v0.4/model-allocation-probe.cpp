// Build against either generated model.h. Count allocation requests during apply,
// excluding model construction and caller-owned Update construction.
#include <cstdlib>
#include <new>
#include <cstddef>
#include <chrono>
#include <iostream>
static bool tracking = false;
static std::size_t allocations = 0, requested_bytes = 0;
void* operator new(std::size_t size) {
    if (void* p = std::malloc(size ? size : 1)) {
        if (tracking) { ++allocations; requested_bytes += size; }
        return p;
    }
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
#include "model.h"
int main() {
    explicit_ui::Model model;
    std::vector<explicit_ui::Update> updates{{"count", std::int64_t{0}}};
    auto run = [&](const char* name, bool change) {
        allocations = requested_bytes = 0;
        const auto start = std::chrono::steady_clock::now();
        for (std::int64_t i = 1; i <= 1000; ++i) {
            updates[0].value = change ? i : std::int64_t{1000};
            tracking = true; model.apply(updates); tracking = false;
        }
        const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-start).count();
        std::cout << "{\"case\":\"" << name << "\",\"iterations\":1000,\"allocations\":" << allocations
                  << ",\"requested_bytes\":" << requested_bytes << ",\"elapsed_ms\":" << elapsed << "}\n";
    };
    run("changed_count", true);
    run("unchanged_count", false);
    updates[0] = {"query", std::string()};
    allocations = requested_bytes = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        updates[0].value = std::string(i % 2 ? "" : "99");
        tracking = true; model.apply(updates); tracking = false;
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-start).count();
    std::cout << "{\"case\":\"changed_query\",\"iterations\":100,\"allocations\":" << allocations
              << ",\"requested_bytes\":" << requested_bytes << ",\"elapsed_ms\":" << elapsed << "}\n";
}
