"""Regenerate deterministic 10000-row demo AST."""
import json
from pathlib import Path

def ref(name): return {'ref': name}
def lit(value, typ): return {'literal': value, 'type': typ}
def op(name, *args): return {'op': name, 'args': list(args)}
doc = {
 'version': 1,
 'persist': ['count', 'query', 'offset'],
 'states': [
  {'id': 'count', 'type': 'int', 'initial': 0},
  {'id': 'query', 'type': 'text', 'initial': ''},
  {'id': 'offset', 'type': 'int', 'initial': 0},
  {'id': 'unrelated', 'type': 'int', 'initial': 0},
  {'id': 'items', 'type': 'texts', 'initial': [f'Item {i:05d}' for i in range(10000)]}],
 'derived': [
  {'id': 'title', 'type': 'text', 'expr': op('concat', lit('Count: ', 'text'), op('to_text', ref('count')))},
  {'id': 'filtered', 'type': 'texts', 'expr': op('filter', ref('items'), ref('query'))},
  {'id': 'visible', 'type': 'texts', 'expr': op('window', ref('filtered'), ref('offset'), lit(12, 'int'))}],
 'views': [{'id': 'counter', 'source': 'title'}, {'id': 'list', 'source': 'visible'}],
 'transactions': [
  {'label': 'unchanged', 'set': {'count': lit(0, 'int')}},
  {'label': 'unrelated', 'set': {'unrelated': lit(42, 'int')}},
  {'label': 'increment', 'set': {'count': op('add', ref('count'), lit(1, 'int'))}},
  {'label': 'batch_filter_scroll', 'set': {'query': lit('99', 'text'), 'offset': lit(2, 'int')}},
  {'label': 'same_filter_result', 'set': {'query': lit('Item 00099', 'text'), 'offset': lit(0, 'int')}},
  {'label': 'identical_filter_output', 'set': {'query': lit('00099', 'text')}},
  {'label': 'empty_batch', 'set': {}}]}
Path(__file__).with_name('counter_list.eui.json').write_text(json.dumps(doc, ensure_ascii=False, indent=2) + '\n')
