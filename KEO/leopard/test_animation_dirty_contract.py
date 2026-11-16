"""Pinned files and verbatim production admission extraction; no process access."""
from pathlib import Path
import hashlib
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, "D:/DEV/Kenshi/tools/_native_python")
import pefile

source_path = ROOT / "src/fixes/world/animation_dirty_patch.cpp"
source = source_path.read_text(encoding="utf-8")
native_path = Path("D:/SteamLibrary/steamapps/common/Kenshi/OgreMain_x64.dll")
native = pefile.PE(str(native_path))
native_hash = hashlib.sha256(native.__data__).hexdigest()
assert native_hash == "7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6"
assert native.FILE_HEADER.TimeDateStamp == 0x5CA5F929
assert native.OPTIONAL_HEADER.SizeOfImage == 0x9C9000
game_path = Path("D:/SteamLibrary/steamapps/common/Kenshi/RE_Kenshi/Kenshi_x64.exe")
game = pefile.PE(str(game_path))
game_hash = hashlib.sha256(game.__data__).hexdigest()
assert game_hash == "504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e"
assert game.FILE_HEADER.TimeDateStamp == 0x65D604D7
assert game.OPTIONAL_HEADER.SizeOfImage == 0x232C000

arrays = {
    name: bytes(int(value, 16) for value in values.split(","))
    for name, values in re.findall(
        r"static const unsigned char (proof\d+)\[\]=\{([^}]+)\}", source
    )
}
rows = re.findall(r"\{(0x[0-9a-f]+),(proof\d+),sizeof\(\2\)\}", source)
expected = [(0x2A2D0, 155), (0x2BA20, 111), (0xCE60, 334), (0x2AF70, 98), (0x2B142, 54)]
assert len(rows) == len(expected)
proofs = []
for (text_rva, name), (rva, length) in zip(rows, expected):
    value = arrays[name]
    assert int(text_rva, 16) == rva and len(value) == length
    assert native.get_data(rva, length) == value, name
    # The real combined fixture can arm Append first. Dirty authenticates
    # its independent native windows, with no broad byte masks.
    assert rva + length <= 0x2B182 or rva >= 0x2B187
    proofs.append({"rva": hex(rva), "bytes": length, "sha256": hashlib.sha256(value).hexdigest()})
assert native.get_data(0x2A357, 5) == bytes.fromhex("e8c4160000")
assert 0x2A357 + 5 == 0x2A35C

start = source.index("struct Proof{")
admit_start = source.index("static bool Admit(HMODULE module)", start)
body_start = source.index("{", admit_start)
depth = 0
body_end = None
for offset in range(body_start, len(source)):
    if source[offset] == "{":
        depth += 1
    elif source[offset] == "}":
        depth -= 1
        if depth == 0:
            body_end = offset + 1
            break
assert body_end is not None
extracted = source[start:body_end]
assert "void Install" not in extracted and "static bool Admit" in extracted
output = ROOT / "build/animation_dirty"
output.mkdir(parents=True, exist_ok=True)
(output / "admission.inc").write_text(
    "// Verbatim production proof declarations and Admit; generated after pinned-file checks.\n"
    + extracted + "\n", encoding="utf-8"
)
record = {
    "source": str(source_path),
    "sourceSha256": hashlib.sha256(source_path.read_bytes()).hexdigest(),
    "nativeSha256": native_hash,
    "gameSha256": game_hash,
    "proofs": proofs,
    "compiledAdmissionBody": True,
    "processAccess": False,
    "gameExecution": False,
}
(output / "native-contract.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
print("PASS pinned Dirty image/proofs/caller/Append-window contracts; exact admission body extracted")
