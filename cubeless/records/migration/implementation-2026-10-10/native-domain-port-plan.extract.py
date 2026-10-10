"""Extract only the helper closure needed by the approved nine Material tools.
Does not import or modify the source project. This records lexical extraction;
the generated UE58 translation unit still requires review, compilation and tests.
"""
from pathlib import Path
import hashlib
import json
import re

HERE = Path(__file__).resolve().parent
SOURCE = Path(r"C:\Git\UnrealMCP5_6_1\MCPGameProject\Plugins\UnrealMCP\Source\UnrealMCP\Private\Commands\UnrealMCPMaterialCommands.cpp")
ROOT = HERE.parents[3]
DEST = ROOT / "Plugins/CubelessMCP_UE58/Source/CubelessMCPUE58Editor/Private/Compatibility/UE58/CubelessMaterialLegacy.inc"
text = SOURCE.read_text(encoding="utf-8-sig")
# Mask strings and comments while preserving every character offset/newline.
masked = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', lambda m: ''.join('\n' if c == '\n' else ' ' for c in m.group()), text)
start = masked.index('{', masked.index('namespace')) + 1
end = masked.index('FUnrealMCPMaterialCommands::FUnrealMCPMaterialCommands')
blocks = {}
depth = 0
boundary = start
block_start = None
for pos in range(start, end):
    c = masked[pos]
    if c == '{':
        if depth == 0:
            block_start = boundary
        depth += 1
    elif c == '}':
        depth -= 1
        if depth < 0:
            break
        if depth == 0:
            prefix = masked[block_start:masked.index('{', block_start)]
            name = re.search(r'\b(?:struct|class)\s+(\w+)', prefix)
            if not name:
                name = re.search(r'(\w+)\s*\([^;{}]*\)\s*$', prefix)
            if name:
                finish = pos + 1
                if masked[finish:finish + 1] == ';':
                    finish += 1
                blocks[name.group(1)] = (block_start, finish)
            boundary = pos + 1
    elif c == ';' and depth == 0:
        boundary = pos + 1

names = ['HandleResolveMaterialGraph','HandleListMaterialNodes','HandleAnalyzeMaterialGraph','HandleListMaterialCollectionParameterNodes','HandleGetMaterialParameterCollectionValues','HandleConnectMaterialNodes','HandleConnectMaterialProperty','HandleDeleteMaterialNode','HandleLayoutMaterialNodes']
for name in names:
    match = re.search(r'TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::' + name + r'\(', masked)
    opening = masked.index('{', match.start())
    depth = 1
    pos = opening + 1
    while depth:
        depth += (masked[pos] == '{') - (masked[pos] == '}')
        pos += 1
    blocks[name] = (match.start(), pos)

needed = set(names)
queue = list(names)
while queue:
    name = queue.pop()
    lo, hi = blocks[name]
    for token in re.findall(r'\b\w+\b', masked[lo:hi]):
        if token in blocks and token not in needed:
            needed.add(token)
            queue.append(token)

header = '// Adapted from user-authorized UnrealMCP5_6_1 Material command domain logic.\n// Original: MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPMaterialCommands.cpp\n// Source SHA-256: ' + hashlib.sha256(SOURCE.read_bytes()).hexdigest() + '\n// Only the dependency closure of the nine explicitly wrapped operations is present.\n// Original comments retained; no license notice was present in this source file.\n// This file is included once inside the UE58 compatibility translation unit.\n\n'
chunks = []
manifest = []
for name in sorted(needed, key=lambda item: blocks[item][0]):
    lo, hi = blocks[name]
    code = text[lo:hi].strip()
    code = code.replace('FUnrealMCPMaterialCommands::', '').replace('FUnrealMCPCommonUtils::CreateErrorResponse', 'MaterialApiError')
    chunks.append(code)
    manifest.append({'symbol':name,'source_line':text[:lo].count('\n')+1,'source_end_line':text[:hi].count('\n')+1})
DEST.parent.mkdir(parents=True, exist_ok=True)
with DEST.open('x', encoding='utf-8', newline='\n') as stream:
    stream.write(header + '\n\n'.join(chunks) + '\n')
report = {'source':str(SOURCE),'source_sha256':hashlib.sha256(SOURCE.read_bytes()).hexdigest(),'destination':str(DEST),'symbols':manifest,'excluded_handlers':[m for m in re.findall(r'FUnrealMCPMaterialCommands::(Handle\w+)\(', text) if m not in names]}
with (HERE/'native-domain-port-plan.extraction.json').open('x',encoding='utf-8') as stream:
    json.dump(report,stream,indent=2)
print(json.dumps({'symbols':len(manifest),'generated_lines':len((header+'\n\n'.join(chunks)).splitlines()),'names':sorted(needed),'destination':str(DEST)},indent=2))
