import os, re

def expand(path):
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()
    
    # replace includes
    def repl(m):
        inc_name = m.group(1)
        inc_path = os.path.join(os.path.dirname(path), inc_name)
        if os.path.exists(inc_path):
            with open(inc_path, 'r', encoding='utf-8') as inc_f:
                return f'\n// BEGIN {inc_name}\n' + inc_f.read() + f'\n// END {inc_name}\n'
        return m.group(0)
    
    return re.sub(r'#include\s+"(Spells[^"]+\.inc)"', repl, text)

full_text = expand('c:/EmberEngineB/engine/world/Spells.cpp')

tokens = re.finditer(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\bnamespace\b|\{|\}|;', full_text, flags=re.DOTALL)

scope_stack = []
ns_stack = []

line_no = 1
pos = 0

for m in tokens:
    tok = m.group(0)
    start = m.start()
    line_no += full_text.count('\n', pos, start)
    pos = start
    
    if tok.startswith('//') or tok.startswith('/*') or tok.startswith('"') or tok.startswith("'"):
        continue
    if tok == 'namespace':
        scope_stack.append(('namespace_decl', line_no))
    elif tok == '{':
        if scope_stack and scope_stack[-1][0] == 'namespace_decl':
            decl = scope_stack.pop()
            scope_stack.append(('namespace_body', line_no))
            ns_stack.append((decl[1], len(scope_stack)))
            print(f'Line {line_no}: Opened namespace (started at line {decl[1]}). Total ns open: {len(ns_stack)}')
        else:
            scope_stack.append(('block', line_no))
    elif tok == '}':
        if ns_stack and ns_stack[-1][1] == len(scope_stack):
            opened_at, _ = ns_stack.pop()
            scope_stack.pop()
            print(f'Line {line_no}: Closed namespace opened at line {opened_at}! Total ns open: {len(ns_stack)}')
        elif scope_stack:
            scope_stack.pop()
    elif tok == ';':
        if scope_stack and scope_stack[-1][0] == 'namespace_decl':
            scope_stack.pop()

print(f'Total namespaces still open at end of expanded Spells.cpp: {len(ns_stack)}')
for opened_at, _ in ns_stack:
    print(f'  Unclosed namespace opened at line {opened_at}')
