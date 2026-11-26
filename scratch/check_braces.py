import re, os

def expand(path):
    with open(path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    out = []
    for line_idx, line in enumerate(lines, 1):
        m = re.match(r'#include\s+"(Spells[^"]+\.inc)"', line.strip())
        if m:
            inc_path = os.path.join(os.path.dirname(path), m.group(1))
            with open(inc_path, 'r', encoding='utf-8') as inc_f:
                for inc_idx, inc_line in enumerate(inc_f, 1):
                    out.append((f'{m.group(1)}:{inc_idx}', inc_line))
        else:
            out.append((f'{os.path.basename(path)}:{line_idx}', line))
    return out

expanded = expand('engine/world/Spells.cpp')

stack = []
for loc, line in expanded:
    s = line.strip()
    # remove comments
    s = re.sub(r'//.*', '', s).strip()
    if s.startswith('namespace'):
        # extract namespace name
        parts = s.split()
        ns_name = parts[1] if len(parts) > 1 and parts[1] != '{' else '(anon)'
        stack.append((loc, ns_name))
        print(f'{loc}: PUSH -> current stack: {[n for _, n in stack]}')
    elif s == '}' or s.startswith('}'):
        # does this line close a namespace?
        if '// namespace' in line or (stack and stack[-1][1] == '(anon)' and s == '}'):
            if stack:
                popped = stack.pop()
                print(f'{loc}: POP {popped[1]} -> current stack: {[n for _, n in stack]}')
