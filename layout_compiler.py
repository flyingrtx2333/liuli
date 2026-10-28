#!/usr/bin/env python3
"""Strict fixed-canvas layout AST -> C++ values. No runtime JSON or CSS engine."""
import argparse
import json
from pathlib import Path
from compiler import Compiler, CompileError, fields, require, quoted, no_duplicates


def validate_search_initial(value):
    require(len(value.encode('utf-8')) <= 256,
            'search initial text exceeds 256 UTF-8 bytes')
    require(not any(ord(c) < 32 or ord(c) == 127 or c in '\u2028\u2029' for c in value),
            'search initial text must be single-line without controls')


def list_offsets(nodes, depth=0):
    """Collect legacy search reset targets; normal validation still checks each node."""
    require(depth <= 16, 'flow exceeds node/depth limit')
    result = []
    for node in nodes:
        if type(node) is not dict:
            continue
        if node.get('kind') == 'list' and type(node.get('state')) is str:
            if node['state'] not in result:
                result.append(node['state'])
        children = node.get('children', [])
        if type(children) is list and children:
            for state in list_offsets(children, depth + 1):
                if state not in result:
                    result.append(state)
    return result


def input_resets(widget, states, legacy_offsets):
    if widget['kind'] not in ('search', 'input'):
        return []
    values = widget.get('reset_states', legacy_offsets if widget['kind'] == 'search' else [])
    require(type(values) is list and len(values) <= 200,
            'reset_states must contain at most 200 integer state IDs')
    require(all(type(v) is str and states.get(v) == 'int' for v in values),
            'reset_states must name integer states')
    require(len(values) == len(set(values)), 'duplicate reset_states target')
    return values


def widget_tail(action, enabled_view, resets):
    if not resets:
        return [quoted(action), quoted(enabled_view)] if enabled_view else ([quoted(action)] if action else [])
    return [quoted(action), quoted(enabled_view), 'true',
            '{' + ','.join(quoted(value) for value in resets) + '}']


def compile_layout(source, model_source):
    require(len(source.encode('utf-8')) <= 262144, 'layout exceeds 256 KiB')
    doc = json.loads(source, object_pairs_hook=no_duplicates)
    model = Compiler(json.loads(model_source, object_pairs_hook=no_duplicates))
    model.generate()  # Validate transactions too.
    states = {s['id']: s['type'] for s in model.doc['states']}
    initials = {s['id']: s['initial'] for s in model.doc['states']}
    views = {v['id']: model.symbols[v['source']][1] for v in model.doc['views']}
    actions = {action['id'] for action in model.doc.get('actions', [])}
    if type(doc) is dict and doc.get('version') == 2:
        return compile_flow(doc, states, views, actions, initials)
    fields(doc, ['version', 'width', 'height', 'widgets'])
    require(type(doc['version']) is int and doc['version'] == 1, 'unsupported layout version')
    require(type(doc['width']) is int and type(doc['height']) is int and (doc['width'],doc['height']) == (1120,840), 'v0.3 requires a 1120 x 840 logical canvas')
    require(type(doc['widgets']) is list and 0 < len(doc['widgets']) <= 100, 'expected 1..100 widgets')
    ids = set()
    out = ['#pragma once', '#include "layout.h"', 'namespace explicit_ui {', 'inline Layout make_layout(int width=1120,int height=840) { return scale_layout({1120,840,{']
    search_count=0
    legacy_offsets=list_offsets(doc['widgets'])
    for w in doc['widgets']:
        require(type(w) is dict, 'widget must be an object')
        kind = w.get('kind')
        require(kind in ('text','button','search','input','list'), 'unknown widget kind')
        common = ['id','kind','rect']
        required = {'text':['text','binding'], 'button':['text','state','delta'], 'search':['text','state'], 'input':['text','state'], 'list':['binding','state','delta']}[kind]
        if kind=='button' and 'action' in w:required=['text','action']
        fields(w, common+required+(['enabled_view'] if kind=='button' and 'enabled_view' in w else [])+(['reset_states'] if kind in ('search','input') and 'reset_states' in w else []))
        require(type(w['id']) is str and 0<len(w['id'])<=64 and '\0' not in w['id'] and w['id'] not in ids, 'invalid or duplicate widget id')
        ids.add(w['id'])
        rect=w['rect']
        require(type(rect) is list and len(rect)==4 and all(type(n) is int for n in rect), 'rect must contain four integers')
        x,y,width,height=rect
        require(x>=0 and y>=0 and width>=24 and height>=24 and x+width<=1120 and y+height<=840, 'widget rectangle outside logical canvas or too small')
        text,binding,state,delta=w.get('text',''),w.get('binding',''),w.get('state',''),w.get('delta',0)
        require(type(text) is str and len(text)<=1024 and '\0' not in text, 'invalid widget text')
        require(type(binding) is str and type(state) is str, 'invalid binding/state')
        if kind=='text':
            require(not binding or views.get(binding)=='text', 'text binding must name a text view')
        if kind=='list':
            require(views.get(binding)=='texts', 'list binding must name a texts view')
        enabled_view=w.get('enabled_view','')
        if 'enabled_view' in w:require(type(enabled_view) is str and views.get(enabled_view)=='bool','enabled_view must name a bool view')
        action=w.get('action','')
        if kind=='button' and 'action' in w:
            require(type(action) is str and action in actions, 'button action must name a model action')
        if kind=='list' or (kind=='button' and not action):
            require(states.get(state)=='int', 'action state must name an integer state')
            require(type(delta) is int and -1000<=delta<=1000 and delta!=0, 'action delta must be a nonzero integer in [-1000,1000]')
        if kind=='list':
            require(1<=delta<=100, 'list scroll delta must be in [1,100]')
        if kind in ('search','input'):
            search_count+=1
            require(search_count<=16, 'at most 16 input fields supported')
            require(states.get(state)=='text', 'search state must name a text state')
            validate_search_initial(initials[state])
        out += ['{'+','.join([quoted(w['id']), 'WidgetKind::'+('search' if kind=='input' else kind), *map(str,rect),quoted(text),quoted(binding),quoted(state),str(delta)]+widget_tail(action,enabled_view,input_resets(w,states,legacy_offsets)))+'},']
    out += ['}},width,height); }', '}']
    return '\n'.join(out)+'\n'

def compile_flow(doc, states, views, actions, initials):
    fields(doc, ['version','width','height','root'])
    require(type(doc['version']) is int and doc['version']==2, 'unsupported layout version')
    require(all(type(doc[k]) is int and 320<=doc[k]<=4096 for k in ('width','height')), 'initial window size must be 320..4096')
    ids=set()
    count=0
    searches=0
    legacy_offsets=list_offsets([doc['root']])
    def node(w,depth=0):
        nonlocal count,searches
        count+=1
        require(count<=200 and depth<=16, 'flow exceeds node/depth limit')
        require(type(w) is dict, 'flow node must be object')
        kind=w.get('kind')
        require(kind in ('row','column','text','button','search','input','list'), 'unsupported flow kind')
        common=['kind','basis','grow']
        container=kind in ('row','column')
        extra=['padding','gap','stack_below','children'] if container else ['id']+{'text':['text','binding'],'button':['text','state','delta'],'search':['text','state'], 'input':['text','state'],'list':['binding','state','delta']}[kind]
        if kind=='button' and 'action' in w:extra=['id','text','action']
        fields(w,common+extra+(['enabled_view'] if kind=='button' and 'enabled_view' in w else [])+(['reset_states'] if kind in ('search','input') and 'reset_states' in w else []))
        basis=w['basis']
        require(type(basis) is list and len(basis)==2 and all(type(n) is int and 0<=n<=4096 for n in basis), 'basis must contain two integers in 0..4096')
        require(type(w['grow']) is int and 0<=w['grow']<=1000, 'grow must be integer in 0..1000')
        if container:
            require(all(type(w[k]) is int and 0<=w[k]<=4096 for k in ('padding','gap','stack_below')), 'invalid container spacing/breakpoint')
            require(kind=='row' or w['stack_below']==0,'only rows can stack at a breakpoint')
            require(type(w['children']) is list and 0<len(w['children'])<=100,'container needs 1..100 children')
            children=','.join(node(child,depth+1) for child in w['children'])
            return '{false,{},Axis::'+kind+','+','.join(map(str,[*basis,w['grow'],w['padding'],w['gap'],w['stack_below']]))+',{'+children+'}}'
        require(type(w['id']) is str and 0<len(w['id'])<=64 and '\0' not in w['id'] and w['id'] not in ids,'invalid or duplicate widget id')
        ids.add(w['id'])
        text,binding,state,delta=w.get('text',''),w.get('binding',''),w.get('state',''),w.get('delta',0)
        require(type(text) is str and len(text)<=1024 and '\0' not in text,'invalid widget text')
        require(type(binding) is str and type(state) is str,'invalid binding/state')
        if kind=='text':require(not binding or views.get(binding)=='text','text binding must name a text view')
        if kind=='list':require(views.get(binding)=='texts','list binding must name a texts view')
        enabled_view=w.get('enabled_view','')
        if 'enabled_view' in w:require(type(enabled_view) is str and views.get(enabled_view)=='bool','enabled_view must name a bool view')
        action=w.get('action','')
        if kind=='button' and 'action' in w:
            require(type(action) is str and action in actions, 'button action must name a model action')
        if kind=='list' or (kind=='button' and not action):
            require(states.get(state)=='int','action state must name integer state')
            require(type(delta) is int and -1000<=delta<=1000 and delta!=0,'invalid action delta')
        if kind=='list':require(1<=delta<=100,'list scroll delta must be in [1,100]')
        if kind in ('search','input'):
            searches+=1
            require(searches<=16,'at most 16 input fields supported')
            require(states.get(state)=='text','search state must name text state')
            validate_search_initial(initials[state])
        widget='{'+','.join([quoted(w['id']),'WidgetKind::'+('search' if kind=='input' else kind),'0','0','0','0',quoted(text),quoted(binding),quoted(state),str(delta)]+widget_tail(action,enabled_view,input_resets(w,states,legacy_offsets)))+'}'
        return '{true,'+widget+',Axis::column,'+','.join(map(str,[*basis,w['grow'],0,0,0]))+',{}}'
    tree=node(doc['root'])
    return '#pragma once\n#include "layout.h"\nnamespace explicit_ui {\ninline Layout make_layout(int width='+str(doc['width'])+',int height='+str(doc['height'])+') {\nstatic const Flow root='+tree+';\nreturn resolve_layout(root,width,height);\n}\n}\n'


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input',type=Path)
    parser.add_argument('--model',required=True,type=Path)
    parser.add_argument('-o','--output',required=True,type=Path)
    args=parser.parse_args()
    try:
        result=compile_layout(args.input.read_text(),args.model.read_text())
        args.output.write_text(result)
    except (ValueError, TypeError, KeyError, RecursionError, OSError, UnicodeError) as exc:
        parser.exit(1,'layout compile error: '+str(exc)+'\n')
