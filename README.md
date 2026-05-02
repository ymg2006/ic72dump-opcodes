# PHP 7.2 ionCube Opcode Dumper

## Overview

This project produces a structured opcode dump from PHP source files that have
been encoded with ionCube for PHP 7.2 (NTS, x86, Windows). For each input file
the tool writes two output files:

- **`<name>.opcodes.txt`** — raw `print_r` dump of every `zend_op_array`, used
  as a debugging ground-truth reference.
- **`<name>.opcodes.json`** — structured JSON IR (format `ic72dump-ir-v3`)
  suitable for PHP code reconstruction. This is the primary output.

The tool works by loading the ionCube loader as a Zend extension (as normal),
letting ionCube compile the encoded file into the Zend engine's in-memory op
arrays, then calling our own PHP extension (`php_opcodedump.dll`) to walk those
op arrays, decode any that ionCube has left in their encrypted state, and
serialise the result. A PHP orchestration script (`opcodedump.php`) then
transforms the raw C output into the final IR.

---

## Background — How PHP Executes Code

When PHP processes a `.php` file it compiles the source into a tree of
`zend_op_array` structures (one per file/function/method). Each `zend_op_array`
contains:

| field | meaning |
|---|---|
| `opcodes` (`zend_op *`) | array of VM instructions |
| `last` (`uint32_t`) | number of instructions |
| `literals` (`zval *`) | string/integer constants used by opcodes |
| `last_literal` | count of literals |
| `vars` (`zend_string **`) | compile-time variable names |
| `last_var` | count of CV variables |
| `reserved[0..3]` | extension slots |

Each `zend_op` is 28 bytes (on x86):

| offset | size | field |
|---|---|---|
| 0 | 4 | `handler` — function pointer to the opcode implementation |
| 4 | 4 | `op1` (`znode_op` union) |
| 8 | 4 | `op2` (`znode_op` union) |
| 12 | 4 | `result` (`znode_op` union) |
| 16 | 4 | `extended_value` |
| 20 | 4 | `lineno` |
| 24 | 1 | `opcode` (numeric opcode ID) |
| 25 | 1 | `op1_type` |
| 26 | 1 | `op2_type` |
| 27 | 1 | `result_type` |

---

## What ionCube Does

ionCube replaces the PHP source with a binary-encoded file. When the ionCube
loader Zend extension is active, it intercepts the file during compilation and
produces op arrays that appear valid to PHP but have their opcode streams
encrypted in memory.

The analysis described below was performed using **IDA Pro** on
`ioncube_loader_win_7.2.dll v15.5` (ImageBase = `0x10000000`).

### The Descriptor

For every encoded function/method, ionCube allocates a **descriptor** block.
A pointer to this block is stored in `op_array->reserved[3]`. The descriptor
is an array of `uint32_t` values; the fields that matter to us are:

| index | name used here | set by | content |
|---|---|---|---|
| `[5]` | `desc[5]` | encode | XOR-encoded original opcodes pointer |
| `[6]` | `desc[6]` | encode | pointer to the 28-byte sentinel allocation |
| `[15]` | `desc[15]` | encode / manual fix | base pointer for opcode stream |
| `[16]` | `desc[16]` | encode | start of the real opcodes array |
| `[17]` | `desc[17]` | encode | constant term in the XOR key |
| `[19]` | `desc[19]` | runtime only | runtime state object (NULL until first dispatch) |
| `[27]` | `desc[27]` | encode | saved `last_var` count |

### The Request XOR Key

During RINIT (`sub_100636A0`), ionCube writes the current Unix timestamp into
a global DWORD:

```
dword_100BEEA8  =  time32(0)
```

RVA = `0xBEEA8`. This value is the per-request seed used in all XOR
operations. Every request gets a fresh seed, so the in-memory encoding is
different each time the process handles a request.

### The Encode Step — `sub_100627B0`

Called once per encoded function when the file is first loaded. Receives
`op_array` as `this` (thiscall, x86).

```
v3    = *(this + 10)         ; read field at byte 40 (real opcodes pointer)
desc  = *(this + 31)         ; read reserved[3]  (descriptor)
stub  = emalloc(28)          ; allocate 28-byte sentinel
*(this + 10) = stub          ; replace byte-40 field with sentinel ptr
desc[5] = XOR_ENCODE(v3)     ; store encoded real-opcodes ptr in descriptor
desc[6] = stub               ; store sentinel ptr for later detection
desc[27] = *(this + 9)       ; save last_var (byte 36) into descriptor
*(this + 9) = 0              ; zero last_var
*(this + 20) |= 0x400000     ; set encode marker bit in byte-80 field
desc[16] = real_opcodes_base ; base of the opcodes array
```

After this step the op_array is "encoded":
- byte 40 → sentinel pointer (28 garbage bytes)
- byte 36 → 0
- byte 80 → has bit `0x400000` set
- `reserved[3]` → descriptor

### Important Note on `zend_op_array` Field Offsets (PHP 7.2 NTS x86)

In this loader's view, the field at **byte offset 40** of `zend_op_array` is
the one that holds the real opcodes pointer. This is what the PHP source calls
`T` (a temp-var count for normal functions), but ionCube repurposes it as an
opcodes-pointer slot during its encode/decode cycle. The field at byte 48
(`opcodes` in the PHP struct) is *not* modified by the encode/decode steps.

```
byte 36  (DWORD index  9)  last_var       — zeroed by encode, restored by step2
byte 40  (DWORD index 10)  T / opcodes*  — replaced by sentinel on encode,
                                            set to real ptr by step1 or step2
byte 44  (DWORD index 11)  last           — opcode count, NOT touched by encode
byte 48  (DWORD index 12)  opcodes        — untouched (points to nothing useful)
byte 76  (DWORD index 19)  live_range*   — read by step2 for XOR key
byte 80  (DWORD index 20)  try_catch_array* — high bits used as encode flags
byte 124 (DWORD index 31)  reserved[3]   — descriptor pointer
```

(*field name from PHP source may differ from ionCube's usage)

### Step 1 — Runtime Decode — `sub_10002C30`

Called by the Zend VM dispatch path (`sub_100629D0`) immediately before a
function is about to execute. This is **not** called at compile time.

```c
v2  = sub_10001DD0()         // returns descriptor[19] (runtime state object)
                             // = *(desc + 76)  ← NULL until first dispatch!
v8  = *(this + 31)           // descriptor pointer
*(this + 10) = 0             // zero byte-40
callback = *(v2 + 64)        // decrypt callback function pointer
v4  = callback(this, v2)     // decrypt → writes real opcodes ptr into byte 40
if (v8) efree(v8)            // free descriptor if fn_flags allows
```

After step1, byte 40 holds the **real decrypted opcodes pointer**.

The VM dispatch then immediately reads byte 40 and stores it as
`execute_data->opline` **before** calling step2.

**Critical:** `descriptor[19]` is `NULL` for any op array that has never been
dispatched (i.e., all op arrays at file-compile time). Calling step1 when
`descriptor[19] == NULL` causes an access violation inside the loader.

### Step 2 — Restore Phase — `sub_100628D0`

Called by the VM dispatch after step1. Also callable independently.

Guard check (mirrors our encode detection):
```
if (reserved[3] == NULL  ||  !(byte_80 & 0x400000)) return 0;
```

Restore sequence:
```c
// XOR-decode descriptor[5] to get back the original byte-40 value
key      = dword_100BEEA8 + *(this + 19) + desc[17]
                         // *(this+19) = field at byte 76 = "live_range"
decoded  = BYTE_XOR(desc[5], key)
*(this + 10) = decoded   // restore byte 40 = real opcodes pointer
*(this + 9)  = desc[27]  // restore last_var (byte 36)
desc[15]     = decoded - sizeof(zend_op) * delta  // update base ptr
*(this + 20) &= ~0x400000  // clear encode marker
```

After step2 the op array is fully decoded and byte 40 holds the real opcodes
pointer once more, byte 36 has last_var restored, and the encode-marker bit
is cleared.

---

## Our Approach

We cannot just call step1 at will because `descriptor[19]` is `NULL` at the
point we intercept (after `compile_filename` returns but before any function
has been executed). Calling step1 with NULL runtime state crashes inside the
loader.

We also cannot rely on the Zend VM dispatch to call step1 for us, because we
need the opcodes *before* executing any PHP code from the encoded file.

### Solution: Manual XOR Restore (mirrors step2)

Since we know:
- `desc[5]`  = XOR-encoded opcodes pointer
- `dword_100BEEA8` = request key (readable via RVA `0xBEEA8`)
- `oa[76]`   = the "live_range" field value (still in the op array)
- `desc[17]` = the constant term stored by ionCube

We can reconstruct the XOR key and recover the real opcodes pointer ourselves,
without needing the runtime state at all:

```c
key            = request_key + oa_field_76 + desc[17]
real_ops_ptr   = desc[5] ^ key
oa->opcodes    = (zend_op *) real_ops_ptr
oa->last_var   = desc[27]       // restore last_var
byte_80       &= ~0x400000      // clear encode marker
```

This is implemented in `ic_manual_restore_step2_opcodes()`.

### Encode Detection

We identify encoded op arrays by checking **two** independent markers:

1. **Descriptor present**: `op_array->reserved[3] != NULL`
2. **Marker bit set**: `*(uint32_t*)((char*)oa + 80) & 0x400000`

Either condition alone is also considered (belt-and-suspenders), plus the
legacy PHP 7.1 check `oa->opcodes & 3` is kept as a third fallback so older
ionCube-encoded files still work.

### Step1 Fast Path (when available)

If `descriptor[19]` is **not** NULL (op array has been dispatched at least
once), we *can* call step1. We gate on this with a pre-check:

```c
uint32_t fn_ptr_val = desc[19];
can_call_step1 = (fn_ptr_val != 0);
```

When step1 is available:
1. Call step1 — writes real opcodes pointer into byte 40
2. **Immediately** read byte 40 → `decoded_ops`
3. Call step2 — restores byte 40 to original value, clears marker
4. Assign `oa->opcodes = decoded_ops`

When step1 is NOT available (the common case at dump time):
1. Call step2 if marker is set — clears the marker, restores last_var
2. If step2 didn't restore byte 40 (it may still point to sentinel `desc[6]`),
   call `ic_manual_restore_step2_opcodes()` to XOR-recover the real pointer

### Memory Safety

All access to potentially unmapped pointers is guarded:

- `dasm_ic_committed_readable_ptr(ptr)` — uses `VirtualQuery` to verify a
  pointer is in a committed, readable page before dereferencing it.
- `__try / __except(EXCEPTION_EXECUTE_HANDLER)` — wraps all calls into
  ionCube functions (`step1`, `step2`) and all opcode/literal iteration loops.
- `VirtualProtect` — called before reading opcode/literal arrays to make them
  readable even if ionCube marked pages execute-only.

### Descriptor Cache (`ic_desc_map`)

After step2 runs, `reserved[3]` may be cleared (ionCube frees the descriptor).
To keep the descriptor pointer available for the dump code (which uses it for
jump-target heuristics), we maintain a side table of up to 4096 `(op_array,
descriptor)` pairs populated before step1/step2 are called.

```c
ic_remember_desc(oa, ic_desc);  // save before step1 clears it
// ... later in dump code ...
void *desc = ic_lookup_desc(oa);  // returns reserved[3] or side-table entry
```

### C Extension Dump (`dasm_zend_op_array`)

`dasm_zend_op_array()` in `opcodedump.c` walks the decoded op array and
serialises it into a PHP `zval` array. Beyond the basic opcode/literal/var
fields, the extension extracts:

**Per opcode (`dasm_zend_op`):**
- Numeric code, mnemonic, operand types, operand values, literal references.
- `lineno` — masked with `& ~0x600000u` to strip ionCube-injected high bits
  before export. The raw value has bits `0x600000` set by ionCube; masking
  them recovers the original PHP source line number.
- Jump targets resolved via `ZEND_USE_ABS_JMP_ADDR` logic (32-bit: absolute
  pointer → opline index; 64-bit: relative byte offset).
- `jmpznz_true_opline` — for `ZEND_JMPZNZ` only: the second jump target (the
  non-zero/non-null branch). On 32-bit PHP, `extended_value` holds an absolute
  opline pointer; we resolve it to an opline index using
  `dasm_index_from_address_base()`.

**Per op_array (`dasm_zend_op_array`):**
- `arg_info` — structured array of `zend_arg_info` entries for each parameter.
- `return_type_info` — return type hint, read from `arg_info[-1]` when
  `fn_flags & ZEND_ACC_HAS_RETURN_TYPE (0x40000000)` is set. PHP 7.2 stores
  the return type in the slot immediately before the parameter array.
- `fn_flags`, `num_args`, `required_num_args`, `line_start`, `line_end`.
- `live_range`, `try_catch_array` for exception/finally reconstruction.

**Per class property (`dasm_properties_info`):**
- `default_value` + `has_default_value` — the initial value of the property,
  read from `default_properties_table[OBJ_PROP_TO_NUM(offset)]` for instance
  props or `default_static_members_table[offset]` for static props.
  (Previously a bug: the pointer was computed but the value was never exported.)

---

## IDA Analysis Summary

All addresses relative to `ioncube_loader_win_7.2.dll` (ImageBase `0x10000000`).

| RVA | Name used | Role |
|---|---|---|
| `0x2C30` | `step1` (`sub_10002C30`) | runtime decrypt; crashes if `desc[19]`==NULL |
| `0x628D0` | `step2` (`sub_100628D0`) | XOR restore of opcodes pointer, clears marker |
| `0x27B0` (approx) | `encode` (`sub_100627B0`) | encodes op_array at file-load time |
| `0x636A0` | RINIT (`sub_100636A0`) | sets `dword_100BEEA8 = time32(0)` |
| `0x1DD0` | `sub_10001DD0` | thiscall; returns `*(desc + 76)` = `desc[19]` |
| `0xBEEA8` | `dword_100BEEA8` | per-request XOR seed (written by RINIT) |

---

## PHP IR Layer (`opcodedump.php`)

After the C extension returns the raw dump, `opcodedump.php` transforms it into
a structured JSON IR (format tag `ic72dump-ir-v3`). Key transformations:

### IR top-level structure

```json
{
  "format": "ic72dump-ir-v3",
  "source_file": "...",
  "summary": { "op_array_count": N, "function_count": N, ... },
  "entry": "main",
  "op_arrays": { "<id>": <op_array_ir>, ... },
  "function_index": { "<hex_key>": "<op_array_id>", ... },
  "closure_index": { "<hex_key>": "<op_array_id>", ... },
  "class_index":   { "<class_id>": <class_ir>, ... }
}
```

Op-array IDs use the form `main`, `function:<hex>`,
`closure:<hex>`, or `class:<hex>:method:<hex>`.

### Per op_array IR

Each op_array entry contains:

| field | content |
|---|---|
| `fn_flags_decoded` | `{visibility, is_static, is_abstract, is_final, is_ctor, is_dtor, is_deprecated, is_closure, is_generator, is_variadic, returns_reference, has_return_type}` |
| `num_args` / `required_num_args` | total / non-default parameter count |
| `arg_info` | `[{index, name, type_name, class_name, type_code, pass_by_reference, allow_null, is_variadic, has_default}]` |
| `return_type_info` | `{type_name, class_name, type_code, allow_null}` or `null` |
| `try_catch` | `[{try_op, catch_op, finally_op, finally_end}]` |
| `live_range` | `[{var, start, end}]` |
| `cfg` | `{blocks: [{id, start, end}], edges: [{from, to, kind}]}` |
| `analysis.opcode_stats` | opcode frequency table |
| `analysis.literal_xrefs` | maps literal index → list of opcodes that use it |
| `analysis.sites` | categorised call/return/assignment/lambda/include sites |

### Per opcode IR

| field | content |
|---|---|
| `line` | source line (ionCube bits masked off) |
| `lineno_raw` | unmasked lineno if it differed (null otherwise) |
| `opcode_name` | mnemonic string, e.g. `ZEND_INIT_FCALL` |
| `extended_value_decoded` | per-opcode semantics: INCLUDE_OR_EVAL→string, CAST→type name, CATCH→`{is_last_catch}`, FETCH_*→`{fetch_type, arg}` |
| `jump_targets` | `[opline_index, ...]` — all resolved branch targets |
| `jmpznz_true_opline` | (JMPZNZ only) second jump target index |
| `is_call` / `is_include_or_eval` / `is_lambda_declare` | convenience flags |

### Per class IR

| field | content |
|---|---|
| `ce_flags_decoded` | `{is_interface, is_trait, is_abstract, is_explicit_abstract, is_final, is_anon}` |
| `parent` / `interfaces` / `traits` | inheritance info |
| `properties_info` | per-property: `{flags_decoded, offset, has_default_value, default_value}` |
| `properties_merged` | joined view: instance props sorted by offset, static props by direct index, each with resolved `default_value` |
| `constants_table` | `{value, doc_comment, ce}` per constant |
| `methods` | map of method name → op_array ID |

### String values

All PHP string values (literals, names, doc_comments, default values) are
wrapped in a `safe_value` envelope:

```json
{
  "type": "string",
  "length": 12,
  "printable": true,
  "value": "contacts_group",
  "preview": "contacts_group",
  "hex": "636f6e74616374735f67726f7570",
  "base64": "Y29udGFjdHNfZ3JvdXA=",
  "sha1": "..."
}
```

Non-printable strings have `value: null` and `printable: false`; the hex and
base64 fields always contain the raw bytes.

---

## File Layout

```
ic72dump/
├── dump.bat               ← run this to produce the dump
├── opcodedump.php         ← PHP orchestration + IR builder
├── WRITEUP.md             ← this document
├── runtime/
│   ├── php.exe            ← PHP 7.2.34 NTS x86 CLI binary
│   ├── php7.dll           ← PHP core DLL
│   ├── php.ini            ← loads ionCube + opcodedump extension
│   ├── api-ms-win-crt-*.dll         ← Windows CRT stub DLLs
│   └── ext/
│       ├── php_opcodedump.dll       ← our extension (pre-built)
│		└── ioncube_loader_win_7.2.dll   ← ionCube loader v15.5 (must be present)
├── src/
│   ├── opcodedump.c       ← C source of the extension
│   └── php_opcodedump.h   ← extension header
└── build/
    ├── build.bat          ← recompile the extension
    └── devel/             ← place php-devel-pack here before building
```

---

## Usage

### Produce a Dump

```bat
dump.bat  path\to\encoded.php
```

Output per input file:
- **Console**: brief summary (line range, opcode count, first 12 opcodes)
- **`<name>.opcodes.txt`**: full `print_r` dump (raw C output, debugging reference)
- **`<name>.opcodes.json`**: structured IR in `ic72dump-ir-v3` format (primary output)

Multiple files at once:

```bat
dump.bat  example.php  catalog.php  admin\model\newsletter.php
```

### Rebuild the Extension (if you modify the C source)

1. Download `php-devel-pack-7.2.34-nts-Win32-VC15-x86.zip` from the PHP
   Windows archives and extract it into `build\devel\`.
2. Install Visual Studio Build Tools 2017 (VC15) with x86 native tools.
3. Run:
   ```bat
   cd build
   build.bat
   ```
4. Copy `build\out\php_opcodedump.dll` to `runtime\ext\php_opcodedump.dll`.

---

## Key Constants (hardcoded in `opcodedump.c`)

```c
#define IC_LOADER_NAME      "ioncube_loader_win_7.2.dll"
#define IC_STEP1_RVA        0x2C30u     // sub_10002C30
#define IC_STEP2_RVA        0x628D0u    // sub_100628D0
#define IC_REQUEST_KEY_RVA  0xBEEA8u    // dword_100BEEA8
#define IC_KEY_TABLE_RVA    0xBEE2Cu    // opcode-level key table (partial)
```

If you are targeting a **different version** of the ionCube loader, you must
re-identify these four RVAs in IDA and update the `#define` lines at the top
of `opcodedump.c` before rebuilding.

---

## Limitations

1. **Opcode-level XOR** — ionCube additionally XOR-encrypts individual opcode
   words inside the decrypted array. The key is derived from a per-function
   key stream stored in the loader's key table (`IC_KEY_TABLE_RVA`). We have
   the lookup code in place but the opcode XOR is applied per-word with a
   per-position key; without knowing the exact key-stream format for each
   opcode word it is not fully reversed. The opcode *numbers* (byte 24 of each
   `zend_op`) are readable; the handler pointer and some operand words may
   still be scrambled.

2. **Loader version sensitivity** — all RVAs are specific to the exact build
   of `ioncube_loader_win_7.2.dll` **v15.5** included in this package. A different patch
   level or platform build would have different RVAs.

3. **Windows only** — the ionCube force-decode code uses SEH
   (`__try/__except`) and Windows API (`VirtualQuery`, `VirtualProtect`,
   `GetModuleHandleA`), so the extension only activates on Windows.

4. **No PHP 7.2 return types / JMPZNZ in current test files** — the infrastructure
   for `return_type_info` (from `arg_info[-1]`) and `jmpznz_true_opline`
   (JMPZNZ second target) is implemented and built into the DLL, but the
   specific encoded files tested here do not use PHP 7.2 type hints or produce
   JMPZNZ opcodes, so these fields remain `null`/absent in those dumps. They
   will populate automatically for any file that does use them.

---

## How It All Fits Together — End-to-End Flow

```
dump.bat example.php
  │
  └─► php.exe -c php.ini opcodedump.php example.php
        │
        ├─ PHP startup:
        │    ioncube_loader_win_7.2.dll  loaded as zend_extension
        │      RINIT: dword_100BEEA8 = time32(0)   ← request XOR seed
        │    php_opcodedump.dll          loaded as extension
        │
        ├─ PHP executes opcodedump.php:
        │    dasm_file("example.php")              ← our C function
        │      │
        │      ├─ compile_filename("example.php")
        │      │    ionCube intercepts → reads binary blob → for each fn:
        │      │      encode(op_array):
        │      │        desc[5]      = XOR(real_opcodes_ptr, key)
        │      │        op_array[40] = sentinel (28-byte stub)
        │      │        op_array[80] |= 0x400000
        │      │        reserved[3]  = desc
        │      │    returns main op_array (3 opcodes: FETCH_CLASS/DECLARE/RETURN)
        │      │
        │      ├─ ic_force_decode_all():
        │      │    for each class method op_array:
        │      │      reserved[3] != NULL && byte80 & 0x400000 → encoded!
        │      │      desc[19] == NULL (no dispatch yet) → skip step1
        │      │      step2(oa) → clears marker, may restore byte 40
        │      │      byte40 still == sentinel desc[6]?
        │      │        → ic_manual_restore_step2_opcodes():
        │      │             key = dword_100BEEA8 + oa[76] + desc[17]
        │      │             oa->opcodes = desc[5] ^ key    ← real PHP opcodes!
        │      │             oa->last_var = desc[27]
        │      │             clear marker bit
        │      │
        │      └─ dasm_zend_op_array() × N:
        │           for each decoded op_array → build PHP array with:
        │             opcodes[] (lineno masked, jmpznz_true_opline added)
        │             literals[], vars[], arg_info[], return_type_info
        │             live_range[], try_catch_array[]
        │             properties_info (with default_value from C)
        │
        ├─ opcodedump.php: normalize_dump() + prepare_dump_for_output()
        │    → write example.opcodes.txt  (raw print_r, debugging reference)
        │
        └─ opcodedump.php: build_decompile_ir()
             → fn_flags_decoded, ce_flags_decoded, arg_info_ir, return_type_ir
             → live_range_ir, try_catch_ir
             → build_cfg() per op_array (basic blocks + edges)
             → build_opcode_stats(), build_literal_xrefs(), build_decompile_sites()
             → properties_merged (instance props sorted by offset + default values)
             → write example.opcodes.json  (ic72dump-ir-v3)
```

---

## Verified Results (example.php)

- **1** class decoded: `ModelMarketingContacts` (extends a base `Model` class)
- **113** method op_arrays decoded, **0** `decode_failed` markers
- Main op_array (file body): `ZEND_FETCH_CLASS` → `ZEND_DECLARE_INHERITED_CLASS` →
  `ZEND_RETURN` — correct for a file that only declares one class
- `addSendGroup` — first opcodes recover classic `$this->db->query(...)` pattern:
  `ZEND_RECV` → `ZEND_FETCH_OBJ_R` → `ZEND_INIT_METHOD_CALL` →
  `ZEND_FETCH_CONSTANT` → `ZEND_CONCAT` → ...
- `addSendGroup` — literals correctly show SQL fragments:
  `"INSERT INTO "`, `DB_PREFIX`, `` "contacts_group SET `name` = '" ``,
  `"db"`, `"escape"`, `"name"`, `"description"`, `"getLastId"`
- No class properties on this class (pure DAO/model — all logic in methods);
  `properties_info`, `properties_merged`, `constants_table` are all empty arrays
- `fn_flags_decoded` correctly shows `visibility: "public"` on all methods,
  `has_return_type: false` (no PHP 7.2 return type hints used in this file)
