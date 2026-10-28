#!/usr/bin/env python3
"""Explicit UI v0: strict JSON AST -> dependency-specialized C++17."""
import argparse
import json
import re
from pathlib import Path

class CompileError(ValueError):
    pass

TYPES = {'bool': 'bool', 'int': 'int64_t', 'text': 'std::string', 'texts': 'std::vector<std::string>'}

def require(ok, message):
    if not ok:
        raise CompileError(message)

def fields(obj, keys):
    require(type(obj) is dict and set(obj) == set(keys), 'expected fields: ' + ', '.join(keys))

def quoted(s):
    # Fixed-width octal bytes prevent quotes, newlines, trigraphs and source injection.
    try:
        data = s.encode('utf-8')
    except UnicodeError as exc:
        raise CompileError('invalid Unicode text') from exc
    return 'std::string("' + ''.join('\\%03o' % c for c in data) + '", ' + str(len(data)) + ')'

def literal(value, typ):
    require(typ in TYPES, 'unknown type')
    if typ == 'bool':
        require(type(value) is bool, 'expected boolean')
        return 'true' if value else 'false'
    if typ == 'int':
        require(type(value) is int and -(2**63) <= value < 2**63, 'expected signed 64-bit integer')
        return 'INT64_MIN' if value == -(2**63) else 'int64_t{' + str(value) + '}'
    if typ == 'text':
        require(type(value) is str, 'expected text')
        try:
            return quoted(value)
        except UnicodeError as exc:
            raise CompileError('invalid Unicode text') from exc
    require(type(value) is list and len(value) <= 100000, 'expected texts array (maximum 100000)')
    return 'std::vector<std::string>{' + ','.join(literal(v, 'text') for v in value) + '}'

class Compiler:
    def __init__(self, doc):
        fields(doc, ['version', 'states', 'derived', 'views', 'transactions'] + [key for key in ('actions', 'persist') if type(doc) is dict and key in doc])
        require(type(doc['version']) is int and doc['version'] == 1, 'unsupported version')
        for key in ['states', 'derived', 'views', 'transactions']:
            require(type(doc[key]) is list and len(doc[key]) <= 1000, key + ' must be array, maximum 1000')
        require(type(doc.get('actions', [])) is list and len(doc.get('actions', [])) <= 1000, 'actions must be array, maximum 1000')
        self.doc = doc
        self.symbols = {}
        self.states = []
        self.nodes = []
        self.views = []
        self.budget = 20000
        for state in doc['states']:
            fields(state, ['id', 'type', 'initial'])
            name, typ = self.register(state['id'], state['type'])
            self.states.append((name, typ, literal(state['initial'], typ)))
        # Forward references are intentionally rejected; declaration order is topological.
        for node in doc['derived']:
            fields(node, ['id', 'type', 'expr'])
            typ, code, deps = self.expr(node['expr'])
            require(typ == node['type'], 'derived type mismatch')
            name, _ = self.register(node['id'], typ)
            self.nodes.append((name, typ, code, deps))
        view_ids = set()
        for view in doc['views']:
            fields(view, ['id', 'source'])
            require(type(view['id']) is str and view['id'] not in view_ids, 'invalid or duplicate view id')
            view_ids.add(view['id'])
            require(type(view['source']) is str and view['source'] in self.symbols, 'unknown view source')
            self.views.append((view['id'], self.symbols[view['source']][0]))
        self.persist = doc.get('persist', [])
        require(type(self.persist) is list and len(self.persist) <= len(self.states), 'persist must be an array of state IDs')
        persistent_ids = set()
        declared_states = {state['id'] for state in doc['states']}
        for label in self.persist:
            require(type(label) is str and label in declared_states, 'persist entry must name a state')
            require(label not in persistent_ids, 'duplicate persistent state')
            persistent_ids.add(label)
        self.actions = []
        action_ids = set()
        state_names = {name for name, _, _ in self.states}
        for action in doc.get('actions', []):
            fields(action, ['id', 'set'])
            label = action['id']
            require(type(label) is str and re.fullmatch(r'[A-Za-z][A-Za-z0-9_]{0,63}', label), 'invalid action identifier')
            require(label not in action_ids, 'duplicate action identifier')
            action_ids.add(label)
            require(type(action['set']) is dict, 'action set must be object')
            updates = []
            for target, expr in action['set'].items():
                require(target in self.symbols and self.symbols[target][0] in state_names, 'action target must be state')
                typ, code, _ = self.expr(expr)
                require(typ == self.symbols[target][1], 'action update type mismatch')
                updates.append((target, code))
            self.actions.append((label, updates))

    def register(self, label, typ):
        require(type(label) is str and re.fullmatch(r'[A-Za-z][A-Za-z0-9_]{0,63}', label), 'invalid identifier')
        require(label not in self.symbols, 'duplicate identifier: ' + label)
        require(type(typ) is str and typ in TYPES, 'unknown type')
        result = ('v' + str(len(self.symbols)), typ)
        self.symbols[label] = result
        return result

    def expr(self, expr, depth=0):
        self.budget -= 1
        require(depth <= 64 and self.budget >= 0, 'expression complexity limit exceeded')
        require(type(expr) is dict, 'expression must be object')
        if 'ref' in expr:
            fields(expr, ['ref'])
            ref = expr['ref']
            require(type(ref) is str and ref in self.symbols, 'unknown/forward reference')
            name, typ = self.symbols[ref]
            return typ, name, {name}
        if 'literal' in expr:
            fields(expr, ['literal', 'type'])
            typ = expr['type']
            require(type(typ) is str, 'invalid literal type')
            return typ, literal(expr['literal'], typ), set()
        fields(expr, ['op', 'args'])
        signatures = {'add': (['int', 'int'], 'int', 'checked_add'),
                      'concat': (['text', 'text'], 'text', 'concat'),
                      'to_text': (['int'], 'text', 'std::to_string'),
                      'filter': (['texts', 'text'], 'texts', 'filter'),
                      'window': (['texts', 'int', 'int'], 'texts', 'window'),
                      'append': (['texts', 'text'], 'texts', 'append'),
                      'remove_at': (['texts', 'int'], 'texts', 'remove_at')}
        op = expr['op']
        special = {'not': 1, 'and': 2, 'or': 2, 'eq': 2,
                   'lt': 2, 'le': 2, 'gt': 2, 'ge': 2, 'if': 3}
        require(type(op) is str and (op in signatures or op in special), 'unknown operation')
        if op in special:
            require(type(expr['args']) is list, 'args must be array')
            require(len(expr['args']) == special[op], 'wrong arity')
            args = [self.expr(a, depth + 1) for a in expr['args']]
            ts = [a[0] for a in args]
            codes = ['(' + a[1] + ')' for a in args]
            typ = 'bool'
            if op == 'if':
                require(ts[0] == 'bool' and ts[1] == ts[2], 'operation type mismatch: if')
                typ = ts[1]
                code = codes[0] + ' ? ' + codes[1] + ' : ' + codes[2]
            elif op == 'eq':
                require(ts[0] == ts[1], 'operation type mismatch: eq')
                code = codes[0] + ' == ' + codes[1]
            elif op in ('lt', 'le', 'gt', 'ge'):
                require(ts == ['int', 'int'], 'operation type mismatch: ' + op)
                code = codes[0] + {'lt':' < ', 'le':' <= ', 'gt':' > ', 'ge':' >= '}[op] + codes[1]
            else:
                require(ts == ['bool'] * special[op], 'operation type mismatch: ' + op)
                code = '!' + codes[0] if op == 'not' else codes[0] + (' && ' if op == 'and' else ' || ') + codes[1]
            # Native operators guarantee lazy evaluation; all branches still typecheck.
            return typ, '(' + code + ')', set().union(*(a[2] for a in args))
        require(type(expr['args']) is list, 'args must be array')
        signature, typ, fn = signatures[op]
        require(len(expr['args']) == len(signature), 'wrong arity')
        args = [self.expr(a, depth + 1) for a in expr['args']]
        require([a[0] for a in args] == signature, 'operation type mismatch: ' + op)
        return typ, fn + '(' + ','.join(a[1] for a in args) + ')', set().union(*(a[2] for a in args))

    def generate(self):
        lines = [RUNTIME, 'int main() { try {']
        for name, typ, initial in self.states:
            lines += ['[[maybe_unused]] ' + TYPES[typ] + ' ' + name + ' = ' + initial + ';', '[[maybe_unused]] bool d_' + name + ' = true;']
        for name, typ, code, deps in self.nodes:
            lines += ['[[maybe_unused]] ' + TYPES[typ] + ' ' + name + ' = ' + code + ';',
                      '[[maybe_unused]] bool d_' + name + ' = true;', 'uint64_t e_' + name + ' = 1;']
        lines += ['uint64_t patches = 0, rows = 0, mounts = 0, unmounts = 0;', '{']
        for i, (_, name) in enumerate(self.views):
            lines += ['View<decltype(' + name + ')> view' + str(i) + '(' + name + ', mounts, unmounts); ++patches; rows += row_count(' + name + ');']
        lines += self.snapshot('initial')
        state_names = {s[0] for s in self.states}
        for tx in self.doc['transactions']:
            fields(tx, ['label', 'set'])
            require(type(tx['label']) is str and type(tx['set']) is dict, 'invalid transaction')
            updates = []
            for label, expr in tx['set'].items():
                require(label in self.symbols and self.symbols[label][0] in state_names, 'write target must be state')
                name, typ = self.symbols[label]
                etyp, code, _ = self.expr(expr)
                require(etyp == typ, 'state update type mismatch')
                updates.append((name, code))
            lines += ['{']
            # Stage all expressions before mutation: simultaneous snapshot semantics.
            for name, code in updates:
                lines += ['auto next_' + name + ' = ' + code + ';']
            for name, _ in self.symbols.values():
                lines += ['d_' + name + ' = false;']
            for name, _ in updates:
                lines += ['if (' + name + ' != next_' + name + ') { ' + name + ' = std::move(next_' + name + '); d_' + name + ' = true; }']
            for name, _, code, deps in self.nodes:
                condition = ' || '.join('d_' + dep for dep in sorted(deps)) or 'false'
                lines += ['if (' + condition + ') { ++e_' + name + '; auto next = ' + code + '; if (next != ' + name + ') { ' + name + ' = std::move(next); d_' + name + ' = true; } }']
            for i, (_, name) in enumerate(self.views):
                lines += ['if (d_' + name + ') { view' + str(i) + '.value = ' + name + '; ++patches; rows += row_count(' + name + '); }']
            lines += self.snapshot(tx['label']) + ['}']
        lines += ['}', 'std::cout << "{\\\"lifecycle\\\":{\\\"mounts\\\":" << mounts << ",\\\"unmounts\\\":" << unmounts << "}}\\n";',
                  'return 0; } catch (const std::exception& e) { std::cerr << e.what() << "\\n"; return 2; } }']
        return '\n'.join(lines) + '\n'

    def generate_model(self):
        # Reusable header: all symbols/dependency guards are compiler-derived.
        includes, helpers = RUNTIME.split('template<class T>', 1)
        helpers = 'template<class T>' + helpers
        helpers = re.sub(r'^(int64_t|std::string|std::vector<std::string>|void|uint64_t) ', r'inline \1 ', helpers, flags=re.M)
        lines = ['#pragma once', includes, '#include <memory>\n#include <variant>\n#include "model_api.h"',
                 'namespace explicit_ui { namespace detail {', helpers, '}',
                 'using Value = std::variant<int64_t, std::string, std::vector<std::string>, bool>;',
                 'struct Update { std::string id; Value value; };',
                 'class Model final : public ViewModel {', 'struct Data {']
        # Immutable shared non-scalars make candidate copies cheap while retaining
        # strong rollback: no code mutates a value owned by committed Data.
        types = {name: typ for name, typ in self.symbols.values()}
        def access(name, prefix=''):
            value = prefix + name
            return value if types[name] in ('int', 'bool') else '(*' + value + ')'
        def storage(typ):
            return TYPES[typ] if typ in ('int', 'bool') else 'std::shared_ptr<const ' + TYPES[typ] + '>'
        def own(code, typ):
            return code if typ in ('int', 'bool') else 'std::make_shared<const ' + TYPES[typ] + '>(' + code + ')'
        def expression(code, prefix=''):
            code = re.sub(r'\b(checked_add|concat|filter|window|append|remove_at)\(', r'detail::\1(', code)
            return re.sub(r'\bv\d+\b', lambda m: access(m.group(), prefix), code)
        for name, typ, initial in self.states:
            lines += [storage(typ) + ' ' + name + ' = ' + own(initial, typ) + ';']
        for name, typ, code, deps in self.nodes:
            # A direct immutable reference can share ownership, not duplicate data.
            value = code if typ not in ('int', 'bool') and code in types else own(expression(code), typ)
            lines += [storage(typ) + ' ' + name + ' = ' + value + ';', 'uint64_t e_' + name + ' = 1;']
        lines += ['uint64_t persistence_revision = 0;', 'uint64_t patches = ' + str(len(self.views)) + ';',
                  'uint64_t rows = ' + (' + '.join('detail::row_count(' + access(n) + ')' for _, n in self.views) or '0') + ';',
                  '};', 'std::unique_ptr<Data> data_ = std::make_unique<Data>();', 'public:',
                  'bool apply(const std::vector<Update>& updates) {', 'auto candidate = std::make_unique<Data>(*data_);', '[[maybe_unused]] auto& d = *candidate;']
        for name, _ in self.symbols.values():
            lines += ['[[maybe_unused]] bool d_' + name + ' = false;']
        lines += ['for ([[maybe_unused]] const auto& update : updates) {']
        for i, state in enumerate(self.doc['states']):
            name, typ = self.symbols[state['id']]
            lines += [('if' if i == 0 else 'else if') + ' (update.id == ' + quoted(state['id']) + ') {',
                      'const auto* value = std::get_if<' + TYPES[typ] + '>(&update.value);',
                      'if (!value) throw std::invalid_argument("state type mismatch");']
            if typ == 'text':
                lines += ['require_utf8(*value);']
            if typ == 'texts':
                lines += ['if (value->size() > 100000) throw std::invalid_argument("texts size limit exceeded");']
                lines += ['for (const auto& text : *value) require_utf8(text);']
            lines += ['if (' + access(name, 'd.') + ' != *value) d.' + name + ' = ' + own('*value', typ) + ';', '}']
        lines += [('else ' if self.states else '') + 'throw std::invalid_argument("unknown state");', '}']
        # Last assignment wins; compare final staged values, not intermediate writes.
        for name, typ, _ in self.states:
            comparison = access(name, 'd.') + ' != ' + access(name, 'data_->')
            if typ not in ('int', 'bool'):
                comparison = 'd.' + name + ' != data_->' + name + ' && (' + comparison + ')'
            lines += ['d_' + name + ' = ' + comparison + ';']
        lines += ['if (!(' + (' || '.join('d_' + n for n, _, _ in self.states) or 'false') + ')) return false;']
        for name, typ, code, deps in self.nodes:
            condition = ' || '.join('d_' + dep for dep in sorted(deps)) or 'false'
            if typ not in ('int', 'bool') and code in types:
                # Alias nodes propagate immutable ownership without deep copies.
                value = 'd.' + code
                comparison = 'next != d.' + name + ' && *next != ' + access(name, 'd.')
            else:
                value = expression(code, 'd.')
                comparison = 'next != ' + access(name, 'd.')
            assignment = 'std::move(next)' if typ in ('int', 'bool') or code in types else own('std::move(next)', typ)
            lines += ['if (' + condition + ') { ++d.e_' + name + '; auto next = ' + value + '; if (' + comparison + ') { d.' + name + ' = ' + assignment + '; d_' + name + ' = true; } }']
        lines += ['bool changed = false;']
        for _, name in self.views:
            lines += ['if (d_' + name + ') { changed = true; ++d.patches; d.rows += detail::row_count(' + access(name, 'd.') + '); }']
        # Reuse final-state dirty flags; revision is rollback-safe Data, not the
        # view repaint result. No persistence capture/array copy is needed here.
        persist_changed = ' || '.join('d_' + self.symbols[label][0] for label in self.persist) or 'false'
        lines += ['if (' + persist_changed + ') { if (d.persistence_revision == UINT64_MAX) throw std::overflow_error("persistence revision exhausted"); ++d.persistence_revision; }',
                  'data_.swap(candidate); return changed;', '}',
                  'uint64_t persistence_revision() const noexcept { return data_->persistence_revision; }']
        lines += ['std::vector<Update> prepare_action(const std::string& id) const {', '(void)id;']
        for label, updates in self.actions:
            lines += ['if (id == ' + quoted(label) + ') {', 'std::vector<Update> staged;', 'staged.reserve(' + str(len(updates)) + ');']
            for target, code in updates:
                lines += ['staged.push_back({' + quoted(target) + ', ' + expression(code, 'data_->') + '});']
            lines += ['return staged;', '}']
        lines += ['throw std::invalid_argument("unknown action");', '}']
        lines += ['bool invoke_action(const std::string& id) { return apply(prepare_action(id)); }',
                  'std::vector<Update> persistent_state() const { return {']
        for label in self.persist:
            lines += ['{' + quoted(label) + ',' + access(self.symbols[label][0], 'data_->') + '},']
        lines += ['}; }', 'bool restore_persistent_state(const std::vector<Update>& updates) {',
                  'if (updates.size() != ' + str(len(self.persist)) + ') throw std::invalid_argument("persistent state key count mismatch");']
        for i, label in enumerate(self.persist):
            lines += ['bool seen' + str(i) + ' = false;']
        lines += ['for ([[maybe_unused]] const auto& update : updates) {']
        for i, label in enumerate(self.persist):
            typ = self.symbols[label][1]
            lines += [('if' if i == 0 else 'else if') + ' (update.id == ' + quoted(label) + ') {',
                      'if (seen' + str(i) + ' || !std::holds_alternative<' + TYPES[typ] + '>(update.value)) throw std::invalid_argument("duplicate persistent key or wrong type");',
                      'seen' + str(i) + ' = true;', '}']
        lines += [('else ' if self.persist else '') + 'throw std::invalid_argument("unknown persistent state");', '}',
                  'return apply(updates);', '}']
        types = {name: typ for name, typ in self.symbols.values()}
        for typ, method, return_type in [('text', 'view_text', 'const std::string&'), ('texts', 'view_texts', 'const std::vector<std::string>&'), ('int', 'view_int', 'int64_t'), ('bool', 'view_bool', 'bool')]:
            lines += [return_type + ' ' + method + '(const std::string& id) const' + (' override' if typ != 'int' else '') + ' {', '(void)id;']
            for label, name in self.views:
                if types[name] == typ:
                    lines += ['if (id == ' + quoted(label) + ') return ' + access(name, 'data_->') + ';']
            lines += ['throw std::invalid_argument("unknown view or wrong view type");', '}']
        for typ, method, return_type in [('text', 'state_text', 'const std::string&'), ('texts', 'state_texts', 'const std::vector<std::string>&'), ('int', 'state_int', 'int64_t'), ('bool', 'state_bool', 'bool')]:
            lines += [return_type + ' ' + method + '(const std::string& id) const {', '(void)id;']
            for state in self.doc['states']:
                if state['type'] == typ:
                    lines += ['if (id == ' + quoted(state['id']) + ') return ' + access(self.symbols[state['id']][0], 'data_->') + ';']
            lines += ['throw std::invalid_argument("unknown state or wrong state type");', '}']
        lines += ['Stats stats() const override { return {data_->patches, data_->rows, ' + (' + '.join('data_->e_' + n for n, _, _, _ in self.nodes) or '0') + '}; }',
                  'void snapshot(const std::string& label, uint64_t renders) const {',
                  'std::cout << "{\\\"label\\\":"; detail::emit(label); std::cout << ",\\\"evaluations\\\":{";']
        for i, (label, (name, _)) in enumerate(p for p in self.symbols.items() if p[1][0] in {n[0] for n in self.nodes}):
            lines += [('std::cout << ",";' if i else '') + 'detail::emit(' + quoted(label) + '); std::cout << ":" << data_->e_' + name + ';']
        lines += ['std::cout << "},\\\"views\\\":{";']
        for i, (label, name) in enumerate(self.views):
            lines += [('std::cout << ",";' if i else '') + 'detail::emit(' + quoted(label) + '); std::cout << ":"; detail::emit(' + access(name, 'data_->') + ');']
        lines += ['std::cout << "},\\\"patches\\\":" << data_->patches << ",\\\"rows_materialized\\\":" << data_->rows << ",\\\"renders\\\":" << renders << "}\\n" << std::flush;', '}', '};', '}']
        return '\n'.join(lines) + '\n'

    def snapshot(self, label):
        lines = ['std::cout << "{\\\"label\\\":"; emit(' + quoted(label) + '); std::cout << ",\\\"evaluations\\\":{";']
        for i, (label, (name, _)) in enumerate((p for p in self.symbols.items() if p[1][0] in {n[0] for n in self.nodes})):
            lines += [('std::cout << ",";' if i else '') + 'emit(' + quoted(label) + '); std::cout << ":" << e_' + name + ';']
        lines += ['std::cout << "},\\\"views\\\":{";']
        for i, (label, _) in enumerate(self.views):
            lines += [('std::cout << ",";' if i else '') + 'emit(' + quoted(label) + '); std::cout << ":"; emit(view' + str(i) + '.value);']
        lines += ['std::cout << "},\\\"patches\\\":" << patches << ",\\\"rows_materialized\\\":" << rows << "}\\n";']
        return lines

RUNTIME = r'''#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>
template<class T> struct View {
    T value;
    uint64_t& unmounts;
    View(const T& initial, uint64_t& mounts, uint64_t& unmount_count)
        : value(initial), unmounts(unmount_count) { ++mounts; }
    View(const View&) = delete;
    View& operator=(const View&) = delete;
    ~View() { ++unmounts; }
};
int64_t checked_add(int64_t a, int64_t b) {
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b))
        throw std::overflow_error("integer overflow");
    return a + b;
}
std::string concat(const std::string& a, const std::string& b) { return a + b; }
std::vector<std::string> filter(const std::vector<std::string>& items, const std::string& query) {
    std::vector<std::string> out;
    for (const auto& item : items) if (item.find(query) != std::string::npos) out.push_back(item);
    return out;
}
std::vector<std::string> window(const std::vector<std::string>& items, int64_t start, int64_t count) {
    if (start < 0 || count < 0 || count > 1000) throw std::out_of_range("invalid window: start >= 0, 0 <= count <= 1000 required");
    std::vector<std::string> out;
    auto begin = static_cast<uint64_t>(start);
    for (uint64_t i = begin; i < items.size() && out.size() < static_cast<uint64_t>(count); ++i) out.push_back(items[i]);
    return out;
}
std::vector<std::string> append(const std::vector<std::string>& items, const std::string& item) {
    if (items.size() >= 100000) throw std::length_error("texts size limit exceeded");
    auto out = items;
    out.push_back(item);
    return out;
}
std::vector<std::string> remove_at(const std::vector<std::string>& items, int64_t index) {
    if (index < 0 || static_cast<uint64_t>(index) >= items.size()) throw std::out_of_range("invalid remove_at index");
    auto out = items;
    out.erase(out.begin() + static_cast<std::vector<std::string>::difference_type>(index));
    return out;
}
void emit(const std::string& s) {
    const char* hex = "0123456789abcdef";
    std::cout << '"';
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') std::cout << '\\' << c;
        else if (c < 32) std::cout << "\\u00" << hex[c >> 4] << hex[c & 15];
        else std::cout << c;
    }
    std::cout << '"';
}
void emit(bool value) { std::cout << (value ? "true" : "false"); }
void emit(int64_t n) { std::cout << n; }
void emit(const std::vector<std::string>& items) {
    std::cout << '['; bool first = true;
    for (const auto& item : items) { if (!first) std::cout << ','; first = false; emit(item); }
    std::cout << ']';
}
uint64_t row_count(const std::vector<std::string>& items) { return items.size(); }
uint64_t row_count(const std::string&) { return 0; }
uint64_t row_count(bool) { return 0; }
uint64_t row_count(int64_t) { return 0; }
'''

def no_duplicates(pairs):
    out = {}
    for key, value in pairs:
        require(key not in out, 'duplicate JSON key: ' + key)
        out[key] = value
    return out

def compile_text(source, model=False):
    require(len(source.encode('utf-8')) <= 8 * 1024 * 1024, 'source size limit exceeded')
    try:
        doc = json.loads(source, object_pairs_hook=no_duplicates)
        compiler = Compiler(doc)
        scripted = compiler.generate()  # Validate scripted transactions in both modes.
        return compiler.generate_model() if model else scripted
    except (RecursionError, TypeError) as exc:
        raise CompileError('invalid or excessively nested input') from exc

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('-o', '--output', required=True, type=Path)
    parser.add_argument('--model-header', type=Path, help='also emit reusable persistent Model header')
    args = parser.parse_args()
    try:
        result = compile_text(args.input.read_text(encoding='utf-8'))
        args.output.write_text(result, encoding='utf-8')
        if args.model_header:
            args.model_header.write_text(compile_text(args.input.read_text(encoding='utf-8'), model=True), encoding='utf-8')
    except (CompileError, ValueError, OSError, UnicodeError) as exc:
        parser.exit(1, 'compile error: ' + str(exc) + '\n')

if __name__ == '__main__':
    main()
