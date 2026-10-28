"""Immutable candidate storage preserves alias isolation and strong rollback."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from compiler import compile_text


class ModelStorageTests(unittest.TestCase):
    def test_alias_isolation_noops_and_allocation_failure_rollback(self):
        ref = lambda name: {'ref': name}
        op = lambda name, *args: {'op': name, 'args': list(args)}
        lit = lambda value: {'literal': value, 'type': 'int'}
        doc = {'version': 1,
               'states': [{'id': 'count', 'type': 'int', 'initial': 0},
                          {'id': 'items', 'type': 'texts', 'initial': ['a'*100, 'b'*100]},
                          {'id': 'query', 'type': 'text', 'initial': ''},
                          {'id': 'offset', 'type': 'int', 'initial': 0}],
               'derived': [{'id': 'alias', 'type': 'texts', 'expr': ref('items')},
                           {'id': 'alias2', 'type': 'texts', 'expr': ref('alias')},
                           {'id': 'filtered', 'type': 'texts', 'expr': op('filter', ref('alias2'), ref('query'))},
                           {'id': 'visible', 'type': 'texts', 'expr': op('window', ref('filtered'), ref('offset'), lit(2))},
                           {'id': 'title', 'type': 'text', 'expr': op('to_text', op('add', ref('count'), lit(1)))}],
               'views': [{'id': name, 'source': name} for name in ['alias', 'alias2', 'visible', 'title', 'query']],
               'transactions': []}
        code = r'''
#include <cstdlib>
#include <new>
#include <cassert>
#include <limits>
static long budget = -1;
void* operator new(std::size_t n) {
    if (budget == 0) throw std::bad_alloc();
    if (budget > 0) --budget;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
#include "model.h"
using namespace explicit_ui;
int main() {
    Model model;
    assert(&model.view_texts("alias") == &model.view_texts("alias2"));
    auto* original = &model.view_texts("alias");
    auto initial_stats = model.stats();
    assert(!model.apply({}));
    assert(!model.apply({{"count",int64_t{0}}}));
    assert(&model.view_texts("alias") == original);
    std::vector<std::string> input{std::string(100,'x'),std::string(100,'y')};
    std::vector<Update> updates{{"items",input}};
    assert(model.apply(updates));
    std::get<std::vector<std::string>>(updates[0].value)[0] = "caller mutation";
    input[0] = "another mutation";
    assert(model.view_texts("alias")[0] == std::string(100,'x'));
    assert(&model.view_texts("alias") == &model.view_texts("alias2"));
    original = &model.view_texts("alias");
    auto saved = *original;
    auto stats = model.stats();
    assert(!model.apply({{"items",std::vector<std::string>{"temporary"}}, {"items",saved}}));
    assert(&model.view_texts("alias") == original);
    assert(model.stats().evaluations == stats.evaluations);
    for (int variant = 0; variant < 2; ++variant) {
        try {
            if (variant == 0) model.apply({{"items",std::vector<std::string>{"bad"}}, {"offset",int64_t{-1}}});
            else model.apply({{"items",std::vector<std::string>{"bad"}}, {"count",std::numeric_limits<int64_t>::max()}});
            assert(false);
        } catch (const std::out_of_range&) {} catch (const std::overflow_error&) {}
        assert(&model.view_texts("alias") == original);
        assert(model.view_texts("alias") == saved);
        assert(model.state_int("count") == 0 && model.state_int("offset") == 0);
        assert(model.stats().evaluations == stats.evaluations);
        assert(model.stats().patches == stats.patches);
    }
    // Input validation is not bypassed merely because final writes would be no-ops.
    try { model.apply({{"query",std::string("\xff",1)}, {"query",std::string()}}); assert(false); }
    catch (const std::invalid_argument&) {}
    assert(model.view_text("query").empty());
    // Inject allocation failure at every allocation before a successful transaction.
    bool reached_success = false;
    for (long fail_at = 0; fail_at < 1000; ++fail_at) {
        Model candidate;
        auto* old = &candidate.view_texts("alias");
        const auto before = candidate.stats();
        std::vector<Update> change{{"items",std::vector<std::string>{std::string(100,'z')}}, {"count",int64_t{2}}};
        budget = fail_at;
        try { candidate.apply(change); budget = -1; reached_success = true; }
        catch (const std::bad_alloc&) {
            budget = -1;
            assert(&candidate.view_texts("alias") == old);
            assert(candidate.view_texts("alias")[0] == std::string(100,'a'));
            assert(candidate.state_int("count") == 0);
            assert(candidate.stats().patches == before.patches);
            assert(candidate.stats().evaluations == before.evaluations);
        }
        if (reached_success) { assert(candidate.view_text("title") == "3"); break; }
    }
    assert(reached_success);
    assert(initial_stats.evaluations == 5);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory)
            (p/'model.h').write_text(compile_text(json.dumps(doc), model=True))
            (p/'test.cpp').write_text(code)
            subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-O0', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', str(ROOT/'native'), str(p/'test.cpp'), '-o', str(p/'test')], check=True, capture_output=True)
            subprocess.run([str(p/'test')], check=True, capture_output=True, timeout=10)

if __name__ == '__main__':
    unittest.main()
