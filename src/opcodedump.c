/*
  +----------------------------------------------------------------------+
  | PHP Version 7 - opcodedump extension (PHP 7.2 build)                |
  +----------------------------------------------------------------------+
  | ionCube loader: ioncube_loader_win_7.2.dll  ImageBase=0x10000000    |
  |                                                                      |
  | RVAs (found via IDA analysis of ioncube_loader_win_7.2.dll):        |
  |   IC_STEP1_RVA       = 0x2F50   (sub_10002F50)                     |
  |   IC_STEP2_RVA       = 0x67730  (sub_10067730)                     |
  |   IC_KEY_TABLE_RVA   = 0x0 (not yet found — opcodes XOR skipped)   |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "Zend/zend_vm_opcodes.h"
#include "php_opcodedump.h"
#include <stdint.h>
#ifdef PHP_WIN32
#include <windows.h>
static int dasm_ic_committed_readable_ptr(const void *ptr);
#endif

void dasm_zend_op_array(zval *dst, const zend_op_array *src);

static inline uint32_t dasm_assoc_key_len(size_t key_len)
{
	return key_len > 0 ? (uint32_t)(key_len - 1) : 0;
}

static inline void dasm_add_assoc_long_ex(zval *arg, const char *key, size_t key_len, zend_long value)
{
	add_assoc_long_ex(arg, key, dasm_assoc_key_len(key_len), value);
}

static inline void dasm_add_assoc_bool_ex(zval *arg, const char *key, size_t key_len, zend_bool value)
{
	add_assoc_bool_ex(arg, key, dasm_assoc_key_len(key_len), value);
}

static inline void dasm_add_assoc_zval_ex(zval *arg, const char *key, size_t key_len, zval *value)
{
	add_assoc_zval_ex(arg, key, dasm_assoc_key_len(key_len), value);
}

static inline void dasm_add_assoc_null_ex(zval *arg, const char *key, size_t key_len)
{
	add_assoc_null_ex(arg, key, dasm_assoc_key_len(key_len));
}

static char *dasm_hex_encode(const unsigned char *data, size_t len)
{
	static const char hex[] = "0123456789abcdef";
	char *out;
	size_t i;

	if (data == NULL || len > 1048576) {
		return NULL;
	}

	out = (char *)emalloc((len * 2) + 1);
	for (i = 0; i < len; ++i) {
		out[i * 2] = hex[(data[i] >> 4) & 0x0f];
		out[i * 2 + 1] = hex[data[i] & 0x0f];
	}
	out[len * 2] = '\0';

	return out;
}

static void dasm_add_zend_string_and_hex(zval *dst, const char *field,
                                         const char *hex_field, zend_string *str)
{
	const char *value = NULL;
	size_t value_len = 0;
	char *hex = NULL;

	if (str == NULL) {
		add_assoc_null(dst, field);
		add_assoc_null(dst, hex_field);
		return;
	}

#ifdef PHP_WIN32
	__try {
		if (dasm_ic_committed_readable_ptr(str)) {
			value = ZSTR_VAL(str);
			value_len = ZSTR_LEN(str);
			if (value_len > 1048576 ||
			    !dasm_ic_committed_readable_ptr(value) ||
			    !dasm_ic_committed_readable_ptr(value + value_len)) {
				value = NULL;
				value_len = 0;
			}
		}
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		value = NULL;
		value_len = 0;
	}
#else
	value = ZSTR_VAL(str);
	value_len = ZSTR_LEN(str);
#endif

	if (value == NULL) {
		add_assoc_null(dst, field);
		add_assoc_null(dst, hex_field);
		return;
	}

	add_assoc_stringl(dst, field, (char *)value, value_len);
	hex = dasm_hex_encode((const unsigned char *)value, value_len);
	if (hex) {
		add_assoc_string(dst, hex_field, hex);
		efree(hex);
	} else {
		add_assoc_null(dst, hex_field);
	}
}

#define add_assoc_long_ex dasm_add_assoc_long_ex
#define add_assoc_bool_ex dasm_add_assoc_bool_ex
#define add_assoc_zval_ex dasm_add_assoc_zval_ex
#define add_assoc_null_ex dasm_add_assoc_null_ex

static int le_opcodedump;

void dasm_zend_arg_info(zval *dst, const zend_arg_info *src)
{
	if (src->name == NULL) {
		add_assoc_null(dst, "name");
		add_assoc_long(dst, "name_len", 0);
	} else {
		const char *_name = NULL;
		size_t _name_len = 0;
#ifdef PHP_WIN32
		__try {
			if (dasm_ic_committed_readable_ptr(src->name)) {
				_name = ZSTR_VAL(src->name);
				_name_len = ZSTR_LEN(src->name);
				if (_name_len > 32768 ||
				    !dasm_ic_committed_readable_ptr(_name) ||
				    !dasm_ic_committed_readable_ptr(_name + _name_len)) {
					_name = NULL;
					_name_len = 0;
				}
			}
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			_name = NULL;
			_name_len = 0;
		}
#else
		_name = ZSTR_VAL(src->name);
		_name_len = ZSTR_LEN(src->name);
#endif
		if (_name) {
			add_assoc_stringl(dst, "name", (char *)_name, _name_len);
			add_assoc_long(dst, "name_len", _name_len);
		} else {
			add_assoc_null(dst, "name");
			add_assoc_long(dst, "name_len", 0);
		}
	}

	/* PHP 7.2: zend_arg_info.type is a zend_type (uintptr_t), not class_name/type_hint/allow_null */
	if (ZEND_TYPE_IS_CLASS(src->type)) {
		zend_string *cn = ZEND_TYPE_NAME(src->type);
		const char *_class_name = NULL;
		size_t _class_name_len = 0;
#ifdef PHP_WIN32
		__try {
			if (dasm_ic_committed_readable_ptr(cn)) {
				_class_name = ZSTR_VAL(cn);
				_class_name_len = ZSTR_LEN(cn);
				if (_class_name_len > 32768 ||
				    !dasm_ic_committed_readable_ptr(_class_name) ||
				    !dasm_ic_committed_readable_ptr(_class_name + _class_name_len)) {
					_class_name = NULL;
					_class_name_len = 0;
				}
			}
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			_class_name = NULL;
			_class_name_len = 0;
		}
#else
		_class_name = ZSTR_VAL(cn);
		_class_name_len = ZSTR_LEN(cn);
#endif
		if (_class_name) {
			add_assoc_stringl(dst, "class_name", (char *)_class_name, _class_name_len);
			add_assoc_long(dst, "class_name_len", (zend_long)_class_name_len);
		} else {
			add_assoc_null(dst, "class_name");
			add_assoc_long(dst, "class_name_len", 0);
		}
	} else {
		add_assoc_null(dst, "class_name");
		add_assoc_long(dst, "class_name_len", 0);
	}

	{
		zend_long type_code = ZEND_TYPE_IS_CODE(src->type) ? (zend_long)ZEND_TYPE_CODE(src->type) : 0;
		add_assoc_long(dst, "type", type_code);
		add_assoc_long_ex(dst, ("type_hint"), (sizeof("type_hint")), type_code);
	}
	add_assoc_bool_ex(dst, ("pass_by_reference"), (sizeof("pass_by_reference")), src->pass_by_reference ? 1 : 0);
	add_assoc_bool_ex(dst, ("allow_null"), (sizeof("allow_null")), ZEND_TYPE_ALLOW_NULL(src->type) ? 1 : 0);
	add_assoc_bool_ex(dst, ("is_variadic"), (sizeof("is_variadic")), src->is_variadic ? 1 : 0);
}

void dasm_zend_uint_array(zval *dst, const uint32_t *src)
{
	int len = sizeof(src) / sizeof(uint32_t);
	if (len > 0) {
		int i;
		for (i = 0; i < len; i++) {
			add_next_index_long(dst, src[i]);
		}
	}
}

static const char *dasm_znode_type_name(zend_uchar type)
{
	switch (type) {
		case IS_CONST:   return "IS_CONST";
		case IS_TMP_VAR: return "IS_TMP_VAR";
		case IS_VAR:     return "IS_VAR";
		case IS_UNUSED:  return "IS_UNUSED";
		case IS_CV:      return "IS_CV";
		default:         return NULL;
	}
}

static zend_long dasm_literal_index(const zend_op_array *op_array, const zval *literal)
{
	if (op_array == NULL || op_array->literals == NULL || literal == NULL) {
		return -1;
	}
	if (literal < op_array->literals || literal >= (op_array->literals + op_array->last_literal)) {
		return -1;
	}
	return (zend_long)(literal - op_array->literals);
}

static zend_long dasm_normalize_fn_flags(uint32_t fn_flags)
{
	return (zend_long)(fn_flags & ~ZEND_ACC_DONE_PASS_TWO);
}

static int dasm_operand_is_jump_target(zend_uchar opcode, int operand_index)
{
	switch (opcode) {
		case ZEND_JMP:
		case ZEND_FAST_CALL:
			return operand_index == 1;
		case ZEND_JMPZ:
		case ZEND_JMPNZ:
		case ZEND_JMPZNZ:
		case ZEND_JMPZ_EX:
		case ZEND_JMPNZ_EX:
		case ZEND_FE_RESET_R:
		case ZEND_FE_RESET_RW:
		case ZEND_FE_FETCH_R:
		case ZEND_FE_FETCH_RW:
		case ZEND_JMP_SET:
		case ZEND_COALESCE:
		case ZEND_ASSERT_CHECK:
		case ZEND_CATCH:
			return operand_index == 2;
		default:
			return 0;
	}
}

/* ---- ionCube helper forward decls (Windows-only) ---- */
#ifdef PHP_WIN32
/* IC_LOADER_NAME: DLL loaded by PHP as zend_extension for PHP 7.2 */
#ifndef IC_LOADER_NAME
#define IC_LOADER_NAME     "ioncube_loader_win_7.2.dll"
#endif
/* IC_KEY_TABLE_RVA: TODO — find in ioncube_loader_win_7.2.dll via IDA
 * (look for HIDWORD of xmmword key-table pointer, same pattern as 7.1) */
#ifndef IC_KEY_TABLE_RVA
#define IC_KEY_TABLE_RVA   0xBEE2Cu
#endif
static void *ic_lookup_desc(const zend_op_array *op_array);
static int   dasm_ic_committed_readable_ptr(const void *ptr);
#endif

static zend_long dasm_index_from_address_base(uintptr_t raw, uintptr_t base, uint32_t last)
{
	uintptr_t size;
	if (base == 0 || raw < base || last == 0) {
		return -1;
	}
	size = (uintptr_t)last * sizeof(zend_op);
	if (raw >= base + size || ((raw - base) % sizeof(zend_op)) != 0) {
		return -1;
	}
	return (zend_long)((raw - base) / sizeof(zend_op));
}

#ifdef PHP_WIN32
static zend_long dasm_ic_line_gap_jump_target(const zend_op_array *op_array, zend_long current_index)
{
	zend_long i;
	uint32_t prev_line;

	if (op_array == NULL || op_array->opcodes == NULL ||
	    current_index < 0 || current_index + 2 >= (zend_long)op_array->last) {
		return -1;
	}

	prev_line = op_array->opcodes[current_index + 1].lineno & ~0x600000u;
	if (prev_line == 0) return -1;

	for (i = current_index + 2; i < (zend_long)op_array->last; ++i) {
		uint32_t line = op_array->opcodes[i].lineno & ~0x600000u;
		if (line == 0) continue;
		if (line > prev_line + 1) return i;
		prev_line = line;
	}
	return -1;
}
#endif

static zend_long dasm_jump_target_index(const zend_op_array *op_array, const zend_op *opline,
                                         zend_uchar opcode, int operand_index, znode_op operand)
{
	zend_long current_index;
	zend_long target_index = -1;

	if (op_array == NULL || op_array->opcodes == NULL ||
	    !dasm_operand_is_jump_target(opcode, operand_index)) {
		return -1;
	}

	current_index = (opline != NULL) ? (zend_long)(opline - op_array->opcodes) : -1;

#if ZEND_USE_ABS_JMP_ADDR
	target_index = dasm_index_from_address_base(
	    (uintptr_t)operand.jmp_addr, (uintptr_t)op_array->opcodes, op_array->last);
#else
	{
		const zend_op *target = (const zend_op *)((const char *)&operand + operand.jmp_offset);
		if (target >= op_array->opcodes && target < (op_array->opcodes + op_array->last)) {
			target_index = (zend_long)(target - op_array->opcodes);
		}
	}
#endif

#ifdef PHP_WIN32
	if (opcode == ZEND_JMPZ && operand_index == 2) {
		__try {
			const uint32_t *desc = (const uint32_t *)ic_lookup_desc(op_array);
			if (desc && dasm_ic_committed_readable_ptr(desc)) {
				zend_long line_gap = dasm_ic_line_gap_jump_target(op_array, current_index);
				if (line_gap >= 0) {
					target_index = line_gap;
				}
			}
		} __except(EXCEPTION_EXECUTE_HANDLER) {}
	}
#endif

	return target_index;
}

static zend_long dasm_operand_value(const zend_op_array *op_array, const zend_op *opline,
                                     zend_uchar opcode, int operand_index,
                                     zend_uchar type, znode_op operand, zend_long constant_index)
{
	zend_long jump_target = dasm_jump_target_index(op_array, opline, opcode, operand_index, operand);
	if (jump_target >= 0) return jump_target;
	if (type == IS_CONST && constant_index >= 0) return constant_index;
	return (zend_long)operand.var;
}

static void dasm_add_literal(zval *dst, const char *key,
                              const zend_op_array *op_array, zend_uchar type, znode_op operand)
{
	zend_long literal_index;
	zval literal;

	if (type != IS_CONST || operand.zv == NULL) return;
	literal_index = dasm_literal_index(op_array, operand.zv);
	if (literal_index < 0) return;
	ZVAL_DUP(&literal, operand.zv);
	add_assoc_zval(dst, key, &literal);
}

/* Resolves an IS_CV operand byte-offset to the human-readable variable name
 * stored in op_array->vars[].  The byte offset encodes the CV slot as:
 *   offset = (ZEND_CALL_FRAME_SLOT + cv_index) * sizeof(zval)
 * so we subtract the frame-slot before indexing vars[]. */
static void dasm_add_cv_name(zval *dst, const char *key,
                              const zend_op_array *op_array,
                              zend_uchar type, znode_op operand)
{
	uint32_t cv_slot, cv_index;
	zend_string *var_name;

	if (type != IS_CV) return;
	if (op_array == NULL || op_array->vars == NULL || op_array->last_var == 0) return;

	cv_slot  = (uint32_t)operand.var / (uint32_t)sizeof(zval);
	if (cv_slot < (uint32_t)ZEND_CALL_FRAME_SLOT) return;
	cv_index = cv_slot - (uint32_t)ZEND_CALL_FRAME_SLOT;
	if (cv_index >= (uint32_t)op_array->last_var) return;

	var_name = op_array->vars[cv_index];
	if (var_name == NULL) return;

#ifdef PHP_WIN32
	__try {
		if (!dasm_ic_committed_readable_ptr(var_name)) return;
		{
			const char *_vname = ZSTR_VAL(var_name);
			size_t      _vlen  = ZSTR_LEN(var_name);
			if (_vlen == 0 || _vlen > 32768 ||
			    !dasm_ic_committed_readable_ptr(_vname) ||
			    !dasm_ic_committed_readable_ptr(_vname + _vlen)) return;
			add_assoc_stringl(dst, key, (char *)_vname, _vlen);
		}
	} __except(EXCEPTION_EXECUTE_HANDLER) {}
#else
	add_assoc_stringl(dst, key, ZSTR_VAL(var_name), ZSTR_LEN(var_name));
#endif
}

/* ============================================================
 * ionCube descriptor map  (Windows only)
 * ============================================================ */
#ifdef PHP_WIN32
#ifndef IC_LOADER_NAME
#define IC_LOADER_NAME     "ioncube_loader_win_7.2.dll"
#endif
#ifndef IC_KEY_TABLE_RVA
#define IC_KEY_TABLE_RVA   0xBEE2Cu
#endif
#ifndef IC_REQUEST_KEY_RVA
#define IC_REQUEST_KEY_RVA 0xBEEA8u
#endif
#define IC_DESC_MAP_MAX 4096

static const zend_op_array *ic_desc_map_oa[IC_DESC_MAP_MAX];
static void *ic_desc_map_desc[IC_DESC_MAP_MAX];
static uint32_t ic_desc_map_count = 0;

static void ic_remember_desc(const zend_op_array *op_array, void *desc)
{
	uint32_t i;
	if (op_array == NULL || desc == NULL) return;
	for (i = 0; i < ic_desc_map_count; ++i) {
		if (ic_desc_map_oa[i] == op_array) {
			ic_desc_map_desc[i] = desc;
			return;
		}
	}
	if (ic_desc_map_count < IC_DESC_MAP_MAX) {
		ic_desc_map_oa[ic_desc_map_count]   = op_array;
		ic_desc_map_desc[ic_desc_map_count] = desc;
		ic_desc_map_count++;
	}
}

static void *ic_lookup_desc(const zend_op_array *op_array)
{
	uint32_t i;
	if (op_array == NULL) return NULL;
	if (op_array->reserved[3] != NULL) return op_array->reserved[3];
	for (i = 0; i < ic_desc_map_count; ++i) {
		if (ic_desc_map_oa[i] == op_array) return ic_desc_map_desc[i];
	}
	return NULL;
}

static int dasm_ic_committed_readable_ptr(const void *ptr)
{
	MEMORY_BASIC_INFORMATION mbi;
	if (ptr == NULL || !VirtualQuery(ptr, &mbi, sizeof(mbi))) return 0;
	if (mbi.State != MEM_COMMIT ||
	    mbi.Protect == PAGE_NOACCESS ||
	    mbi.Protect == PAGE_EXECUTE) return 0;
	return 1;
}

static int dasm_ic_key_material_for_index(const zend_op_array *op_array, zend_long op_index,
                                            zend_uchar *key_byte_out, uint32_t *key_dword_out)
{
	HMODULE hLoader;
	uint32_t key_table;
	const uint32_t *desc;
	void *desc_ptr;
	uint32_t key_index;
	uintptr_t key_entry_addr;
	const uint8_t *key_stream;
	const uint32_t *key_stream32;

	if (key_byte_out)  *key_byte_out  = 0;
	if (key_dword_out) *key_dword_out = 0;

	if (op_array == NULL || op_index < 0 || op_index >= (zend_long)op_array->last) return 0;

	if (IC_KEY_TABLE_RVA == 0) return 0; /* RVA not yet filled */

	hLoader = GetModuleHandleA(IC_LOADER_NAME);
	if (!hLoader || !dasm_ic_committed_readable_ptr((const void *)((uintptr_t)hLoader + IC_KEY_TABLE_RVA)))
		return 0;

	desc_ptr = ic_lookup_desc(op_array);
	desc = (const uint32_t *)desc_ptr;
	if (!dasm_ic_committed_readable_ptr(desc)) return 0;

	key_table = *(const uint32_t *)((uintptr_t)hLoader + IC_KEY_TABLE_RVA);
	key_index = desc[1];
	if (key_table == 0 || key_index == 0xFFFFFFFFu || key_index > 4096) return 0;

	key_entry_addr = (uintptr_t)key_table + (uintptr_t)key_index * sizeof(uint32_t);
	if (!dasm_ic_committed_readable_ptr((const void *)key_entry_addr)) return 0;

	key_stream = *(const uint8_t * const *)key_entry_addr;
	if (!dasm_ic_committed_readable_ptr(key_stream + op_index)) return 0;
	key_stream32 = (const uint32_t *)key_stream;
	if (!dasm_ic_committed_readable_ptr(key_stream32 + op_index)) return 0;

	if (key_byte_out)  *key_byte_out  = key_stream[op_index];
	if (key_dword_out) *key_dword_out = key_stream32[op_index];
	return 1;
}

static zend_uchar dasm_ic_display_opcode(const zend_op_array *op_array, const zend_op *opline,
                                          zend_uchar opcode)
{
	zend_uchar decoded = opcode;
	if (op_array == NULL || opline == NULL || op_array->opcodes == NULL) return opcode;
	__try {
		zend_long op_index = (zend_long)(opline - op_array->opcodes);
		zend_uchar key_byte = 0;
		if (op_index < 0 || op_index >= (zend_long)op_array->last) return opcode;
		if (!dasm_ic_key_material_for_index(op_array, op_index, &key_byte, NULL)) return opcode;
		decoded = (zend_uchar)(opcode ^ key_byte);
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		decoded = opcode;
	}
	return decoded;
}
#else
static zend_uchar dasm_ic_display_opcode(const zend_op_array *op_array, const zend_op *opline,
                                          zend_uchar opcode)
{
	return opcode;
}
#endif /* PHP_WIN32 */

/* ============================================================
 * dasm_zend_op
 * ============================================================ */
static void inline dasm_zend_op(zval *dst, const zend_op_array *op_array, const zend_op *src)
{
	const zend_op *raw_src;
	zend_op decoded_src;
	const char *op1_type_name, *op2_type_name, *result_type_name;
	zend_long op1_constant, op2_constant, result_constant;
	zend_long op1_value, op2_value, result_value;
	zend_uchar display_opcode;

	raw_src        = src;
	display_opcode = dasm_ic_display_opcode(op_array, src, src->opcode);
	decoded_src    = *src;
	decoded_src.opcode = display_opcode;
	src = &decoded_src;

	op1_type_name    = dasm_znode_type_name(src->op1_type);
	op2_type_name    = dasm_znode_type_name(src->op2_type);
	result_type_name = dasm_znode_type_name(src->result_type);

	op1_constant    = src->op1.constant;
	op2_constant    = src->op2.constant;
	result_constant = src->result.constant;

	if (src->op1_type == IS_CONST) {
		zend_long li = dasm_literal_index(op_array, src->op1.zv);
		if (li >= 0) op1_constant = li;
	}
	if (src->op2_type == IS_CONST) {
		zend_long li = dasm_literal_index(op_array, src->op2.zv);
		if (li >= 0) op2_constant = li;
	}
	if (src->result_type == IS_CONST) {
		zend_long li = dasm_literal_index(op_array, src->result.zv);
		if (li >= 0) result_constant = li;
	}

	add_assoc_long_ex(dst, ("opcode"),         (sizeof("opcode")),         display_opcode);
	add_assoc_long_ex(dst, ("op1_type"),        (sizeof("op1_type")),        src->op1_type);
	add_assoc_long_ex(dst, ("op2_type"),        (sizeof("op2_type")),        src->op2_type);
	add_assoc_long_ex(dst, ("result_type"),     (sizeof("result_type")),     src->result_type);
	add_assoc_long_ex(dst, ("extended_value"),  (sizeof("extended_value")),  src->extended_value);
	/* Mask ionCube-injected bits from lineno (same mask as line_start/line_end) */
	add_assoc_long_ex(dst, ("lineno"),          (sizeof("lineno")),          src->lineno & ~0x600000u);

	if (zend_get_opcode_name(display_opcode) != NULL) {
		add_assoc_string(dst, "opcode_name", (char *)zend_get_opcode_name(display_opcode));
	}
	if (op1_type_name)    add_assoc_string(dst, "op1_type_name",    (char *)op1_type_name);
	if (op2_type_name)    add_assoc_string(dst, "op2_type_name",    (char *)op2_type_name);
	if (result_type_name) add_assoc_string(dst, "result_type_name", (char *)result_type_name);

	dasm_add_literal(dst, "op1.literal",    op_array, src->op1_type,    src->op1);
	dasm_add_literal(dst, "op2.literal",    op_array, src->op2_type,    src->op2);
	dasm_add_literal(dst, "result.literal", op_array, src->result_type, src->result);

	dasm_add_cv_name(dst, "op1.cv_name",    op_array, src->op1_type,    src->op1);
	dasm_add_cv_name(dst, "op2.cv_name",    op_array, src->op2_type,    src->op2);
	dasm_add_cv_name(dst, "result.cv_name", op_array, src->result_type, src->result);

	result_value = dasm_operand_value(op_array, raw_src, src->opcode, 3, src->result_type, src->result, result_constant);
	op1_value    = dasm_operand_value(op_array, raw_src, src->opcode, 1, src->op1_type,    src->op1,    op1_constant);
	op2_value    = dasm_operand_value(op_array, raw_src, src->opcode, 2, src->op2_type,    src->op2,    op2_constant);

	add_assoc_long_ex(dst, ("result.constant"),   (sizeof("result.constant")),   result_value);
	add_assoc_long_ex(dst, ("result.var"),         (sizeof("result.var")),         result_value);
	add_assoc_long_ex(dst, ("result.num"),         (sizeof("result.num")),         result_value);
	add_assoc_long_ex(dst, ("result.opline_num"),  (sizeof("result.opline_num")),  result_value);
	add_assoc_long_ex(dst, ("op1.constant"),       (sizeof("op1.constant")),       op1_value);
	add_assoc_long_ex(dst, ("op1.var"),            (sizeof("op1.var")),            op1_value);
	add_assoc_long_ex(dst, ("op1.num"),            (sizeof("op1.num")),            op1_value);
	add_assoc_long_ex(dst, ("op1.opline_num"),     (sizeof("op1.opline_num")),     op1_value);
	add_assoc_long_ex(dst, ("op2.constant"),       (sizeof("op2.constant")),       op2_value);
	add_assoc_long_ex(dst, ("op2.var"),            (sizeof("op2.var")),            op2_value);
	add_assoc_long_ex(dst, ("op2.num"),            (sizeof("op2.num")),            op2_value);
	add_assoc_long_ex(dst, ("op2.opline_num"),     (sizeof("op2.opline_num")),     op2_value);
	add_assoc_long(dst, "handler", (zend_long)(uintptr_t)src->handler);

	/* ZEND_JMPZNZ: extended_value is the second jump target (non-zero/non-null branch).
	 * On 32-bit (ZEND_USE_ABS_JMP_ADDR=1): stored as absolute opline pointer cast to uint32_t.
	 * On 64-bit (ZEND_USE_ABS_JMP_ADDR=0): stored as relative byte offset from current opline. */
	if (display_opcode == ZEND_JMPZNZ) {
		zend_long ev_index = -1;
#if ZEND_USE_ABS_JMP_ADDR
		ev_index = dasm_index_from_address_base(
			(uintptr_t)(uint32_t)raw_src->extended_value,
			(uintptr_t)op_array->opcodes, op_array->last);
#else
		{
			const zend_op *ev_target = (const zend_op *)((const char *)raw_src + (int32_t)raw_src->extended_value);
			if (op_array->opcodes && ev_target >= op_array->opcodes &&
			    ev_target < (op_array->opcodes + op_array->last)) {
				ev_index = (zend_long)(ev_target - op_array->opcodes);
			}
		}
#endif
		add_assoc_long(dst, "jmpznz_true_opline", ev_index);
	}
}

#ifdef PHP_WIN32
static void inline dasm_zend_op_minimal(zval *dst, const zend_op *src)
{
	__try { add_assoc_long_ex(dst, ("opcode"),        (sizeof("opcode")),        src->opcode); } __except(EXCEPTION_EXECUTE_HANDLER) {}
	__try { add_assoc_long_ex(dst, ("op1_type"),      (sizeof("op1_type")),      src->op1_type); } __except(EXCEPTION_EXECUTE_HANDLER) {}
	__try { add_assoc_long_ex(dst, ("op2_type"),      (sizeof("op2_type")),      src->op2_type); } __except(EXCEPTION_EXECUTE_HANDLER) {}
	__try { add_assoc_long_ex(dst, ("result_type"),   (sizeof("result_type")),   src->result_type); } __except(EXCEPTION_EXECUTE_HANDLER) {}
	__try { add_assoc_long_ex(dst, ("extended_value"),(sizeof("extended_value")),src->extended_value); } __except(EXCEPTION_EXECUTE_HANDLER) {}
	__try { add_assoc_long_ex(dst, ("lineno"),        (sizeof("lineno")),        src->lineno); } __except(EXCEPTION_EXECUTE_HANDLER) {}
	__try { add_assoc_long(dst, "handler", (zend_long)(uintptr_t)src->handler); } __except(EXCEPTION_EXECUTE_HANDLER) {}
	add_assoc_bool(dst, "decode_failed", 1);
}
#endif

/* ============================================================
 * Live range (PHP 7.1+)
 * ============================================================ */
#if defined(ZEND_ENGINE_7_1)
static void inline dasm_zend_live_range(zval *dst, const zend_live_range *src)
{
	add_assoc_long_ex(dst, ("var"),   (sizeof("var")),   src->var);
	add_assoc_long_ex(dst, ("start"), (sizeof("start")), src->start);
	add_assoc_long_ex(dst, ("end"),   (sizeof("end")),   src->end);
}
#endif

static void inline dasm_zend_try_catch_element(zval *dst, const zend_try_catch_element *src)
{
	add_assoc_long_ex(dst, ("try_op"),     (sizeof("try_op")),     src->try_op);
	add_assoc_long_ex(dst, ("catch_op"),   (sizeof("catch_op")),   src->catch_op);
	add_assoc_long_ex(dst, ("finally_op"), (sizeof("finally_op")), src->finally_op);
	add_assoc_long_ex(dst, ("finally_end"),(sizeof("finally_end")),src->finally_end);
}

static void inline dasm_HashTable(zval *dst, HashTable *src)
{
	zend_ulong num_key;
	zend_string *str_key;
	zval *val;
	ZEND_HASH_FOREACH_KEY_VAL(src, num_key, str_key, val) {
		zval zv;
		ZVAL_COPY_VALUE(&zv, val);
		if (str_key) {
			add_assoc_zval_ex(dst, ZSTR_VAL(str_key), ZSTR_LEN(str_key) + 1, &zv);
		} else {
			add_index_zval(dst, num_key, &zv);
		}
	} ZEND_HASH_FOREACH_END();
}

static void inline dasm_function_table(zval *dst, const HashTable *src)
{
	zend_function *zif;
	zend_ulong num_key;
	zend_string *str_key;
	ZEND_HASH_FOREACH_KEY_PTR((HashTable *)src, num_key, str_key, zif) {
		if (zif->common.type == ZEND_USER_FUNCTION) {
			zval zv, array;
			zend_op_array *op_array;
			array_init(&zv);
			add_assoc_long(&zv, "type", zif->type);
			op_array = &(zif->op_array);
			dasm_zend_op_array(&zv, op_array);
			array_init(&array);
			if (str_key) {
				dasm_add_zend_string_and_hex(&array, "function_table_key",
				                             "function_table_key_hex", str_key);
			} else {
				add_assoc_long(&array, "function_table_key", (zend_long)num_key);
				add_assoc_null(&array, "function_table_key_hex");
			}
			add_assoc_zval(&array, "op_array", &zv);
			if (str_key) {
				add_assoc_zval_ex(dst, ZSTR_VAL(str_key), ZSTR_LEN(str_key) + 1, &array);
			} else {
				add_index_zval(dst, num_key, &array);
			}
		}
	} ZEND_HASH_FOREACH_END();
}

static void dasm_add_parent_op_array_info(zval *dst, const zend_op_array *parent)
{
	if (parent == NULL) {
		add_assoc_null(dst, "parent_function_name");
		add_assoc_null(dst, "parent_function_name_hex");
		add_assoc_null(dst, "parent_filename");
		add_assoc_null(dst, "parent_filename_hex");
		add_assoc_null(dst, "parent_scope");
		add_assoc_null(dst, "parent_scope_hex");
		return;
	}

	dasm_add_zend_string_and_hex(dst, "parent_function_name",
	                             "parent_function_name_hex", parent->function_name);
	dasm_add_zend_string_and_hex(dst, "parent_filename",
	                             "parent_filename_hex", parent->filename);
	if (parent->scope) {
		dasm_add_zend_string_and_hex(dst, "parent_scope",
		                             "parent_scope_hex", parent->scope->name);
	} else {
		add_assoc_null(dst, "parent_scope");
		add_assoc_null(dst, "parent_scope_hex");
	}
}

static void inline dasm_lambda_closures(zval *dst, const zend_op_array *parent)
{
	zval closures, report;
	uint32_t declared = 0;
	uint32_t dumped = 0;
	HashTable *function_table = CG(function_table) ? CG(function_table) : EG(function_table);
	int i;

	array_init(&closures);

	if (parent && parent->opcodes && parent->last > 0 && parent->last < 65536 &&
	    parent->literals && parent->last_literal > 0 && parent->last_literal < 65536) {
		for (i = 0; i < (int)parent->last; ++i) {
			const zend_op *opline = &(parent->opcodes[i]);
			zend_uchar opcode = dasm_ic_display_opcode(parent, opline, opline->opcode);
			zend_long literal_index = -1;
			zend_string *lambda_id = NULL;
			zval *zfunc = NULL;
			zend_function *func = NULL;
			zval entry;

			if (opcode != ZEND_DECLARE_LAMBDA_FUNCTION) {
				continue;
			}

			declared++;
			array_init(&entry);
			add_assoc_long(&entry, "opcode_index", (zend_long)i);
			add_assoc_long(&entry, "opcode", (zend_long)opcode);

			if (opline->op1_type == IS_CONST) {
				literal_index = dasm_literal_index(parent, opline->op1.zv);
			}
			add_assoc_long(&entry, "literal_index", literal_index);

			if (literal_index >= 0 && literal_index < (zend_long)parent->last_literal &&
			    Z_TYPE(parent->literals[literal_index]) == IS_STRING) {
				lambda_id = Z_STR(parent->literals[literal_index]);
				dasm_add_zend_string_and_hex(&entry, "lambda_id", "lambda_id_hex", lambda_id);
			} else {
				add_assoc_null(&entry, "lambda_id");
				add_assoc_null(&entry, "lambda_id_hex");
			}

			dasm_add_parent_op_array_info(&entry, parent);

			if (function_table && lambda_id) {
#ifdef PHP_WIN32
				__try {
					zfunc = zend_hash_find(function_table, lambda_id);
				} __except(EXCEPTION_EXECUTE_HANDLER) {
					zfunc = NULL;
				}
#else
				zfunc = zend_hash_find(function_table, lambda_id);
#endif
			}

			if (zfunc && Z_TYPE_P(zfunc) == IS_PTR) {
				func = (zend_function *)Z_PTR_P(zfunc);
			} else if (zfunc && Z_TYPE_P(zfunc) == IS_INDIRECT) {
				zval *indirect = Z_INDIRECT_P(zfunc);
				if (indirect && Z_TYPE_P(indirect) == IS_PTR) {
					func = (zend_function *)Z_PTR_P(indirect);
				}
			}

			if (func && func->type == ZEND_USER_FUNCTION) {
				zval op_array_dump;
				array_init(&op_array_dump);
#ifdef PHP_WIN32
				__try {
					dasm_zend_op_array(&op_array_dump, &(func->op_array));
					add_assoc_zval(&entry, "op_array", &op_array_dump);
					dumped++;
				} __except(EXCEPTION_EXECUTE_HANDLER) {
					zval_ptr_dtor(&op_array_dump);
					add_assoc_null(&entry, "op_array");
				}
#else
				dasm_zend_op_array(&op_array_dump, &(func->op_array));
				add_assoc_zval(&entry, "op_array", &op_array_dump);
				dumped++;
#endif
			} else {
				add_assoc_null(&entry, "op_array");
			}

			add_next_index_zval(&closures, &entry);
		}
	}

	array_init(&report);
	add_assoc_long(&report, "declared_lambdas", declared);
	add_assoc_long(&report, "dumped_closure_op_arrays", dumped);
	add_assoc_long(&report, "missing", declared >= dumped ? declared - dumped : 0);

	add_assoc_zval(dst, "closures", &closures);
	add_assoc_zval(dst, "closure_report", &report);
}

static void inline _dasm_properties_info(zval *dst, const zend_property_info *src)
{
	if (src->name == NULL) {
		add_assoc_null(dst, "name");
		add_assoc_long(dst, "name_len", 0);
	} else {
		add_assoc_string(dst, "name", ZSTR_VAL(src->name));
		add_assoc_long(dst, "name_len", ZSTR_LEN(src->name));
	}
	if (src->doc_comment == NULL) {
		add_assoc_null(dst, "doc_comment");
		add_assoc_long(dst, "doc_comment_len", 0);
	} else {
		add_assoc_string(dst, "doc_comment", ZSTR_VAL(src->doc_comment));
		add_assoc_long(dst, "doc_comment_len", ZSTR_LEN(src->doc_comment));
	}
	add_assoc_long(dst, "offset", src->offset);
	add_assoc_long(dst, "flags",  src->flags);
	if (src->ce && src->ce->name) {
		add_assoc_string(dst, "ce", ZSTR_VAL(src->ce->name));
	} else {
		add_assoc_null(dst, "ce");
	}
}

static void inline dasm_properties_info(zval *dst, zend_class_entry *ce, int statics)
{
	zend_property_info *prop_info;
	zend_string *key;
	ZEND_HASH_FOREACH_STR_KEY_PTR(&ce->properties_info, key, prop_info) {
		zval *prop = NULL;
		if (statics && (prop_info->flags & ZEND_ACC_STATIC) != 0) {
			prop = &ce->default_static_members_table[prop_info->offset];
		} else if (!statics && (prop_info->flags & ZEND_ACC_STATIC) == 0) {
			prop = &ce->default_properties_table[OBJ_PROP_TO_NUM(prop_info->offset)];
		}
		if (!prop) continue;
		zval zv;
		array_init(&zv);
		_dasm_properties_info(&zv, prop_info);

		/* Export the resolved default value — prop was computed but previously unused (bug). */
#ifdef PHP_WIN32
		__try {
			if (dasm_ic_committed_readable_ptr(prop) && Z_TYPE_P(prop) != IS_UNDEF) {
				zval prop_copy;
				ZVAL_COPY_VALUE(&prop_copy, prop);
				add_assoc_zval(&zv, "default_value", &prop_copy);
				add_assoc_bool(&zv, "has_default_value", 1);
			} else {
				add_assoc_null(&zv, "default_value");
				add_assoc_bool(&zv, "has_default_value", 0);
			}
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			add_assoc_null(&zv, "default_value");
			add_assoc_bool(&zv, "has_default_value", 0);
		}
#else
		if (Z_TYPE_P(prop) != IS_UNDEF) {
			zval prop_copy;
			ZVAL_COPY_VALUE(&prop_copy, prop);
			add_assoc_zval(&zv, "default_value", &prop_copy);
			add_assoc_bool(&zv, "has_default_value", 1);
		} else {
			add_assoc_null(&zv, "default_value");
			add_assoc_bool(&zv, "has_default_value", 0);
		}
#endif
		add_assoc_zval_ex(dst, ZSTR_VAL(prop_info->name), ZSTR_LEN(prop_info->name) + 1, &zv);
	} ZEND_HASH_FOREACH_END();
}

/* Export class constants (name => {value, doc_comment, ce}) */
static void inline dasm_constants_table(zval *dst, zend_class_entry *ce)
{
	zend_string          *con_key;
	zend_class_constant  *c;
	if (!ce) return;
	ZEND_HASH_FOREACH_STR_KEY_PTR(&ce->constants_table, con_key, c) {
		zval zv, val_copy;
		if (!con_key || !c) continue;
#ifdef PHP_WIN32
		if (!dasm_ic_committed_readable_ptr(c)) continue;
#endif
		array_init(&zv);
		ZVAL_COPY_VALUE(&val_copy, &c->value);
		add_assoc_zval_ex(&zv, ("value"), (sizeof("value")), &val_copy);
		if (c->doc_comment
#ifdef PHP_WIN32
		    && dasm_ic_committed_readable_ptr(c->doc_comment)
#endif
		) {
			add_assoc_string(&zv, "doc_comment", ZSTR_VAL(c->doc_comment));
		} else {
			add_assoc_null(&zv, "doc_comment");
		}
		if (c->ce && c->ce->name
#ifdef PHP_WIN32
		    && dasm_ic_committed_readable_ptr(c->ce)
		    && dasm_ic_committed_readable_ptr(c->ce->name)
#endif
		) {
			add_assoc_string(&zv, "ce", ZSTR_VAL(c->ce->name));
		} else {
			add_assoc_null(&zv, "ce");
		}
		add_assoc_zval_ex(dst, ZSTR_VAL(con_key), ZSTR_LEN(con_key) + 1, &zv);
	} ZEND_HASH_FOREACH_END();
}

/* Export the list of interface names a class implements */
static void inline dasm_interfaces_list(zval *dst, zend_class_entry *ce)
{
	uint32_t i;
	if (!ce || !ce->interfaces || ce->num_interfaces == 0) return;
	for (i = 0; i < ce->num_interfaces; i++) {
		zend_class_entry *iface = ce->interfaces[i];
		if (!iface || !iface->name) continue;
#ifdef PHP_WIN32
		__try {
			if (dasm_ic_committed_readable_ptr(iface) &&
			    dasm_ic_committed_readable_ptr(iface->name)) {
				add_next_index_string(dst, ZSTR_VAL(iface->name));
			}
		} __except(EXCEPTION_EXECUTE_HANDLER) {}
#else
		add_next_index_string(dst, ZSTR_VAL(iface->name));
#endif
	}
}

/* Export the list of trait names used by a class */
static void inline dasm_traits_list(zval *dst, zend_class_entry *ce)
{
	uint32_t i;
	if (!ce || !ce->traits || ce->num_traits == 0) return;
	for (i = 0; i < ce->num_traits; i++) {
		zend_class_entry *trait = ce->traits[i];
		if (!trait || !trait->name) continue;
#ifdef PHP_WIN32
		__try {
			if (dasm_ic_committed_readable_ptr(trait) &&
			    dasm_ic_committed_readable_ptr(trait->name)) {
				add_next_index_string(dst, ZSTR_VAL(trait->name));
			}
		} __except(EXCEPTION_EXECUTE_HANDLER) {}
#else
		add_next_index_string(dst, ZSTR_VAL(trait->name));
#endif
	}
}

static void inline dasm_zend_class_entry(zval *dst, zend_class_entry *src)
{
	add_assoc_long_ex(dst, ("type"), (sizeof("type")), src->type);
	if (src->name == NULL) {
		add_assoc_null(dst, "name");
		add_assoc_long(dst, "name_len", 0);
	} else {
		add_assoc_string(dst, "name", ZSTR_VAL(src->name));
		add_assoc_long(dst, "name_len", ZSTR_LEN(src->name));
	}
	if (src->parent) {
		add_assoc_string(dst, "parent", ZSTR_VAL(src->parent->name));
	} else {
		add_assoc_null_ex(dst, ("parent"), (sizeof("parent")));
	}
	add_assoc_long_ex(dst, ("refcount"),                    (sizeof("refcount")),                    src->refcount);
	add_assoc_long_ex(dst, ("ce_flags"),                    (sizeof("ce_flags")),                    src->ce_flags);
	add_assoc_long_ex(dst, ("default_properties_count"),    (sizeof("default_properties_count")),    src->default_properties_count);
	add_assoc_long_ex(dst, ("default_static_members_count"),(sizeof("default_static_members_count")),src->default_static_members_count);

	if (src->default_properties_table) {
		zval table;
		int i;
		array_init(&table);
		for (i = 0; i < src->default_properties_count; i++) {
			zval zv;
			ZVAL_COPY_VALUE(&zv, src->default_properties_table + i);
			add_next_index_zval(&table, &zv);
		}
		add_assoc_zval(dst, "default_properties_table", &table);
	} else {
		add_assoc_null_ex(dst, ("default_properties_table"), (sizeof("default_properties_table")));
	}

	if (src->default_static_members_table) {
		zval table;
		int i;
		array_init(&table);
		for (i = 0; i < src->default_static_members_count; i++) {
			zval zv;
			ZVAL_COPY_VALUE(&zv, src->default_static_members_table + i);
			add_next_index_zval(&table, &zv);
		}
		add_assoc_zval(dst, "default_static_members_table", &table);
	} else {
		add_assoc_null_ex(dst, ("default_static_members_table"), (sizeof("default_static_members_table")));
	}

	do {
		zval zv;
		array_init(&zv);
		dasm_function_table(&zv, &src->function_table);
		add_assoc_zval(dst, "function_table", &zv);
	} while (0);

	do {
		zval zv;
		array_init(&zv);
		dasm_properties_info(&zv, src, 0);
		dasm_properties_info(&zv, src, 1);
		add_assoc_zval(dst, "properties_info", &zv);
	} while (0);

	do {
		zval zv;
		array_init(&zv);
		dasm_constants_table(&zv, src);
		add_assoc_zval_ex(dst, ("constants_table"), (sizeof("constants_table")), &zv);
	} while (0);

	do {
		zval zv;
		array_init(&zv);
		dasm_interfaces_list(&zv, src);
		add_assoc_zval_ex(dst, ("interfaces"), (sizeof("interfaces")), &zv);
	} while (0);

	do {
		zval zv;
		array_init(&zv);
		dasm_traits_list(&zv, src);
		add_assoc_zval_ex(dst, ("traits"), (sizeof("traits")), &zv);
	} while (0);
}

static void inline dasm_class_table(zval *dst, const HashTable *src)
{
	zend_class_entry *ce;
	ZEND_HASH_FOREACH_PTR((HashTable *)src, ce) {
		if (ce->type == ZEND_USER_CLASS) {
			zval zv;
			array_init(&zv);
			dasm_zend_class_entry(&zv, ce);
			add_assoc_zval_ex(dst, ZSTR_VAL(ce->name), ZSTR_LEN(ce->name) + 1, &zv);
		}
	} ZEND_HASH_FOREACH_END();
}

/* ============================================================
 * dasm_zend_op_array  (main dump function)
 * ============================================================ */
void dasm_zend_op_array(zval *dst, const zend_op_array *src)
{
	zend_op_array _ic_fixup;

	/* ionCube sentinel: opcodes field is a small odd integer before decode */
	if (src && src->opcodes && ((uintptr_t)(src->opcodes) & 3)) {
		const zend_op_array *_real = NULL;
		void *_ic_desc = NULL;
#ifdef PHP_WIN32
		__try {
			if (src->reserved[3]) {
				const uint32_t *_desc = (const uint32_t *)src->reserved[3];
				_ic_desc = src->reserved[3];
				uint32_t _fn_ptr_val = _desc[19];
				if (_fn_ptr_val) {
					const uint32_t *_fn_ptr = (const uint32_t *)_fn_ptr_val;
					uint32_t _real_oa_addr = _fn_ptr[10];
					if (_real_oa_addr) {
						const zend_op_array *_cand = (const zend_op_array *)_real_oa_addr;
						if (_cand->type == ZEND_USER_FUNCTION &&
						    _cand->last > 0 && _cand->last < 65536 &&
						    _cand->opcodes != NULL) {
							_real = _cand;
							ic_remember_desc(_cand, _ic_desc);
						}
					}
				}
			}
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			_real = NULL;
		}
#endif
		if (_real) {
			_ic_fixup = *_real;
			_ic_fixup.reserved[3] = _ic_desc;
			src = &_ic_fixup;
		} else {
			memset(&_ic_fixup, 0, sizeof(_ic_fixup));
			_ic_fixup.type          = src->type;
			_ic_fixup.function_name = src->function_name;
			_ic_fixup.fn_flags      = src->fn_flags & ~ZEND_ACC_DONE_PASS_TWO;
			_ic_fixup.filename      = src->filename;
			src = &_ic_fixup;
		}
	}

#ifdef PHP_WIN32
	/* If force-decode could only restore descriptor state, dump the saved
	 * original opcode stream instead of the 28-byte ionCube sentinel. */
	if (src && src->reserved[3]) {
		__try {
			const uint32_t *_desc = (const uint32_t *)src->reserved[3];
			if (dasm_ic_committed_readable_ptr(_desc) &&
			    _desc[6] && (uintptr_t)src->opcodes == (uintptr_t)_desc[6]) {
				HMODULE _hLoader = GetModuleHandleA(IC_LOADER_NAME);
				uint32_t _decoded = _desc[15];
				if (_hLoader &&
				    dasm_ic_committed_readable_ptr((const void *)((uintptr_t)_hLoader + IC_REQUEST_KEY_RVA))) {
					uint32_t _request_key = *(const uint32_t *)((uintptr_t)_hLoader + IC_REQUEST_KEY_RVA);
					uint32_t _raw_line_start = *(const uint32_t *)((const char *)src + 76);
					uint32_t _restore_key = _request_key + _raw_line_start + _desc[17];
					_decoded = _desc[5] ^ _restore_key;
				}
				if (_decoded) {
					_ic_fixup = *src;
					_ic_fixup.opcodes = (zend_op *)(uintptr_t)_decoded;
					if (_desc[27] > 0 && _desc[27] < 65536) _ic_fixup.last = _desc[27];
					_ic_fixup.line_end &= ~0x400000u;
					src = &_ic_fixup;
				}
			}
		} __except(EXCEPTION_EXECUTE_HANDLER) {}
	}
#endif

	add_assoc_long_ex(dst, ("type"), (sizeof("type")), src->type);

	if (src->function_name == NULL) {
		add_assoc_null(dst, "function_name");
	} else {
		const char *_fn = NULL;
		size_t _fn_len = 0;
#ifdef PHP_WIN32
		__try {
			if (dasm_ic_committed_readable_ptr(src->function_name)) {
				_fn = ZSTR_VAL(src->function_name);
				_fn_len = ZSTR_LEN(src->function_name);
				if (_fn_len > 32768 ||
				    !dasm_ic_committed_readable_ptr(_fn) ||
				    !dasm_ic_committed_readable_ptr(_fn + _fn_len)) {
					_fn = NULL;
					_fn_len = 0;
				}
			}
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			_fn = NULL;
			_fn_len = 0;
		}
#else
		_fn = ZSTR_VAL(src->function_name);
		_fn_len = ZSTR_LEN(src->function_name);
#endif
		if (_fn) {
			add_assoc_stringl(dst, "function_name", (char *)_fn, _fn_len);
		} else {
			add_assoc_null(dst, "function_name");
		}
	}

	add_assoc_long_ex(dst, ("fn_flags"), (sizeof("fn_flags")), dasm_normalize_fn_flags(src->fn_flags));

	if (src->arg_info && src->num_args > 0 && src->num_args < 256
#ifdef PHP_WIN32
	    && dasm_ic_committed_readable_ptr(src->arg_info)
	    && dasm_ic_committed_readable_ptr(src->arg_info + src->num_args - 1)
#endif
	) {
		int i;
		zval arr;
		array_init(&arr);
		for (i = 0; i < (int)src->num_args; ++i) {
			zval zv;
			array_init(&zv);
#ifdef PHP_WIN32
			__try { dasm_zend_arg_info(&zv, &(src->arg_info[i])); }
			__except(EXCEPTION_EXECUTE_HANDLER) { add_assoc_null(&zv, "name"); }
#else
			dasm_zend_arg_info(&zv, &(src->arg_info[i]));
#endif
			add_next_index_zval(&arr, &zv);
		}
		add_assoc_zval(dst, "arg_info", &arr);
	} else {
		add_assoc_null(dst, "arg_info");
	}

	/* Return type hint: PHP 7.2 stores it at arg_info[-1] when ZEND_ACC_HAS_RETURN_TYPE (0x40000000) is set.
	 * arg_info is allocated with an extra leading slot for the return info even when num_args == 0. */
	if ((src->fn_flags & ZEND_ACC_HAS_RETURN_TYPE) && src->arg_info != NULL
#ifdef PHP_WIN32
	    && dasm_ic_committed_readable_ptr(src->arg_info - 1)
#endif
	) {
		zval ret_zv;
		array_init(&ret_zv);
#ifdef PHP_WIN32
		__try { dasm_zend_arg_info(&ret_zv, src->arg_info - 1); }
		__except(EXCEPTION_EXECUTE_HANDLER) { add_assoc_null(&ret_zv, "name"); }
#else
		dasm_zend_arg_info(&ret_zv, src->arg_info - 1);
#endif
		add_assoc_zval(dst, "return_type_info", &ret_zv);
	} else {
		add_assoc_null(dst, "return_type_info");
	}

	add_assoc_long_ex(dst, ("num_args"),          (sizeof("num_args")),          src->num_args);
	add_assoc_long_ex(dst, ("required_num_args"), (sizeof("required_num_args")), src->required_num_args);

	if (src->refcount) {
		uint32_t _rc = 0;
#ifdef PHP_WIN32
		__try { _rc = src->refcount[0]; } __except(EXCEPTION_EXECUTE_HANDLER) { _rc = 0; }
#else
		_rc = src->refcount[0];
#endif
		add_assoc_long_ex(dst, ("refcount"), (sizeof("refcount")), _rc);
	} else {
		add_assoc_null_ex(dst, ("refcount"), (sizeof("refcount")));
	}

	/* Literals */
#ifdef PHP_WIN32
	if (src->literals && src->last_literal > 0 && src->last_literal < 65536) {
		SIZE_T _lit_size = (SIZE_T)(src->last_literal) * 16 + 64;
		DWORD _lit_old = 0;
		int _lit_unlocked = VirtualProtect((LPVOID)src->literals, _lit_size, PAGE_EXECUTE_READWRITE, &_lit_old);
		if (!_lit_unlocked)
			_lit_unlocked = VirtualProtect((LPVOID)src->literals, _lit_size, PAGE_READWRITE, &_lit_old);
		int i;
		zval arr;
		array_init(&arr);
		for (i = 0; i < src->last_literal; ++i) {
			zval zv;
			int _lm = 0;
			__try { ((volatile uint8_t *)src->literals)[i * 16 + 8]; _lm = 1; }
			__except(EXCEPTION_EXECUTE_HANDLER) { _lm = 0; }
			if (!_lm) break;
			array_init(&zv);
			__try { ZVAL_COPY_VALUE(&zv, &(src->literals[i])); }
			__except(EXCEPTION_EXECUTE_HANDLER) { ZVAL_NULL(&zv); }
			add_next_index_zval(&arr, &zv);
		}
		if (_lit_unlocked) VirtualProtect((LPVOID)src->literals, _lit_size, _lit_old, &_lit_old);
		add_assoc_zval_ex(dst, ("literals"), (sizeof("literals")), &arr);
	} else {
		add_assoc_null_ex(dst, ("literals"), (sizeof("literals")));
	}
#else
	if (src->literals && src->last_literal > 0 && src->last_literal < 65536) {
		int i;
		zval arr;
		array_init(&arr);
		for (i = 0; i < src->last_literal; ++i) {
			zval zv;
			array_init(&zv);
			ZVAL_COPY_VALUE(&zv, &(src->literals[i]));
			add_next_index_zval(&arr, &zv);
		}
		add_assoc_zval_ex(dst, ("literals"), (sizeof("literals")), &arr);
	} else {
		add_assoc_null_ex(dst, ("literals"), (sizeof("literals")));
	}
#endif

	add_assoc_long_ex(dst, ("last_literal"), (sizeof("last_literal")), src->last_literal);

	/* Opcodes */
#ifdef PHP_WIN32
	if (src->opcodes && src->last > 0 && src->last < 65536) {
		SIZE_T _op_size = (SIZE_T)(src->last) * sizeof(zend_op) + 128;
		DWORD _op_old = 0;
		int _op_unlocked = VirtualProtect((LPVOID)src->opcodes, _op_size, PAGE_EXECUTE_READWRITE, &_op_old);
		DWORD _lit2_old = 0;
		int _lit2_unlocked = 0;
		if (src->literals && src->last_literal > 0) {
			SIZE_T _lit2_size = (SIZE_T)(src->last_literal) * 16 + 64;
			_lit2_unlocked = VirtualProtect((LPVOID)src->literals, _lit2_size, PAGE_EXECUTE_READWRITE, &_lit2_old);
		}
		int i;
		zval arr;
		array_init(&arr);
		for (i = 0; i < (int)src->last; ++i) {
			zval zv;
			int _om = 0;
			__try { volatile zend_uchar _opcode_probe = src->opcodes[i].opcode; (void)_opcode_probe; _om = 1; }
			__except(EXCEPTION_EXECUTE_HANDLER) { _om = 0; }
			if (!_om) break;
			array_init(&zv);
			__try { dasm_zend_op(&zv, src, &(src->opcodes[i])); }
			__except(EXCEPTION_EXECUTE_HANDLER) { dasm_zend_op_minimal(&zv, &(src->opcodes[i])); }
			add_next_index_zval(&arr, &zv);
		}
		if (_op_unlocked)   VirtualProtect((LPVOID)src->opcodes,  _op_size,  _op_old,   &_op_old);
		if (_lit2_unlocked) VirtualProtect((LPVOID)src->literals, (SIZE_T)(src->last_literal)*16+64, _lit2_old, &_lit2_old);
		add_assoc_zval_ex(dst, ("opcodes"), (sizeof("opcodes")), &arr);
	} else {
		add_assoc_null_ex(dst, ("opcodes"), (sizeof("opcodes")));
	}
#else
	if (src->opcodes && src->last > 0 && src->last < 65536) {
		int i;
		zval arr;
		array_init(&arr);
		for (i = 0; i < (int)src->last; ++i) {
			zval zv;
			array_init(&zv);
			dasm_zend_op(&zv, src, &(src->opcodes[i]));
			add_next_index_zval(&arr, &zv);
		}
		add_assoc_zval_ex(dst, ("opcodes"), (sizeof("opcodes")), &arr);
	} else {
		add_assoc_null_ex(dst, ("opcodes"), (sizeof("opcodes")));
	}
#endif

	add_assoc_long_ex(dst, ("last"), (sizeof("last")), src->last);
	add_assoc_long_ex(dst, ("T"),    (sizeof("T")),    src->T);

#if defined(ZEND_ENGINE_7_1)
	if (src->live_range && src->last_live_range > 0 && src->last_live_range < 65536
#ifdef PHP_WIN32
	    && dasm_ic_committed_readable_ptr(src->live_range)
	    && dasm_ic_committed_readable_ptr(src->live_range + src->last_live_range - 1)
#endif
	) {
		int i;
		zval arr;
		array_init(&arr);
		for (i = 0; i < src->last_live_range; ++i) {
			zval zv;
			array_init(&zv);
#ifdef PHP_WIN32
			__try { dasm_zend_live_range(&zv, &(src->live_range[i])); }
			__except(EXCEPTION_EXECUTE_HANDLER) { add_assoc_null(&zv, "var"); }
#else
			dasm_zend_live_range(&zv, &(src->live_range[i]));
#endif
			add_next_index_zval(&arr, &zv);
		}
		add_assoc_zval_ex(dst, ("live_range"),      (sizeof("live_range")),      &arr);
	} else {
		add_assoc_null_ex(dst, ("live_range"),      (sizeof("live_range")));
	}
	add_assoc_long_ex(dst, ("last_live_range"), (sizeof("last_live_range")), src->last_live_range);
#endif

	if (src->try_catch_array && src->last_try_catch > 0 && src->last_try_catch < 65536
#ifdef PHP_WIN32
	    && dasm_ic_committed_readable_ptr(src->try_catch_array)
	    && dasm_ic_committed_readable_ptr(src->try_catch_array + src->last_try_catch - 1)
#endif
	) {
		int i;
		zval arr;
		array_init(&arr);
		for (i = 0; i < src->last_try_catch; ++i) {
			zval zv;
			array_init(&zv);
#ifdef PHP_WIN32
			__try { dasm_zend_try_catch_element(&zv, &(src->try_catch_array[i])); }
			__except(EXCEPTION_EXECUTE_HANDLER) { add_assoc_null(&zv, "try_op"); }
#else
			dasm_zend_try_catch_element(&zv, &(src->try_catch_array[i]));
#endif
			add_next_index_zval(&arr, &zv);
		}
		add_assoc_zval_ex(dst, ("try_catch_array"), (sizeof("try_catch_array")), &arr);
	} else {
		add_assoc_null_ex(dst, ("try_catch_array"), (sizeof("try_catch_array")));
	}
	add_assoc_long_ex(dst, ("last_try_catch"), (sizeof("last_try_catch")), src->last_try_catch);

	if (src->static_variables) {
		zval zv;
		array_init(&zv);
		dasm_HashTable(&zv, src->static_variables);
		add_assoc_zval_ex(dst, ("static_variables"), (sizeof("static_variables")), &zv);
	} else {
		add_assoc_null_ex(dst, ("static_variables"), (sizeof("static_variables")));
	}

	if (src->filename == NULL) {
		add_assoc_null(dst, "filename");
	} else {
		const char *_fname = NULL;
		size_t _fname_len = 0;
#ifdef PHP_WIN32
		__try {
			if (dasm_ic_committed_readable_ptr(src->filename)) {
				_fname = ZSTR_VAL(src->filename);
				_fname_len = ZSTR_LEN(src->filename);
				if (_fname_len > 32768 ||
				    !dasm_ic_committed_readable_ptr(_fname) ||
				    !dasm_ic_committed_readable_ptr(_fname + _fname_len)) {
					_fname = NULL;
					_fname_len = 0;
				}
			}
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			_fname = NULL;
			_fname_len = 0;
		}
#else
		_fname = ZSTR_VAL(src->filename);
		_fname_len = ZSTR_LEN(src->filename);
#endif
		if (_fname) {
			add_assoc_stringl(dst, "filename", (char *)_fname, _fname_len);
		} else {
			add_assoc_string(dst, "filename", "(ionCube-protected)");
		}
	}

	if (src->doc_comment == NULL) {
		add_assoc_null(dst, "doc_comment");
		add_assoc_long(dst, "doc_comment_len", 0);
	} else {
		const char *_doc = NULL;
		size_t _doc_len = 0;
#ifdef PHP_WIN32
		__try {
			if (dasm_ic_committed_readable_ptr(src->doc_comment)) {
				_doc = ZSTR_VAL(src->doc_comment);
				_doc_len = ZSTR_LEN(src->doc_comment);
				if (_doc_len > 1048576 ||
				    !dasm_ic_committed_readable_ptr(_doc) ||
				    !dasm_ic_committed_readable_ptr(_doc + _doc_len)) {
					_doc = NULL;
					_doc_len = 0;
				}
			}
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			_doc = NULL;
			_doc_len = 0;
		}
#else
		_doc = ZSTR_VAL(src->doc_comment);
		_doc_len = ZSTR_LEN(src->doc_comment);
#endif
		if (_doc) {
			add_assoc_stringl(dst, "doc_comment", (char *)_doc, _doc_len);
			add_assoc_long(dst, "doc_comment_len", _doc_len);
		} else {
			add_assoc_null(dst, "doc_comment");
			add_assoc_long(dst, "doc_comment_len", 0);
		}
	}

	add_assoc_long_ex(dst, ("line_start"),    (sizeof("line_start")),    src->line_start & ~0x600000u);
	add_assoc_long_ex(dst, ("line_end"),      (sizeof("line_end")),      src->line_end   & ~0x600000u);
	add_assoc_long_ex(dst, ("early_binding"), (sizeof("early_binding")), src->early_binding);
	add_assoc_long_ex(dst, ("cache_size"),    (sizeof("cache_size")),    src->cache_size);

	/* Variables */
#ifdef PHP_WIN32
	{
		int _var_mem_ok = 0;
		if (src->vars && src->last_var > 0 && src->last_var < 65536) {
			__try { (void)src->vars[0]; _var_mem_ok = 1; } __except(EXCEPTION_EXECUTE_HANDLER) { _var_mem_ok = 0; }
		}
		if (_var_mem_ok) {
			int i;
			zval arr;
			array_init(&arr);
			for (i = 0; i < src->last_var; ++i) {
				zend_string *_vs = NULL;
				int _vm = 0;
				const char *_vname = NULL;
				size_t _vname_len = 0;
				__try {
					_vs = src->vars[i];
					if (_vs && dasm_ic_committed_readable_ptr(_vs)) {
						_vname = ZSTR_VAL(_vs);
						_vname_len = ZSTR_LEN(_vs);
						if (_vname_len > 32768 ||
						    !dasm_ic_committed_readable_ptr(_vname) ||
						    !dasm_ic_committed_readable_ptr(_vname + _vname_len)) {
							_vname = NULL;
							_vname_len = 0;
						}
					}
					_vm = 1;
				} __except(EXCEPTION_EXECUTE_HANDLER) { _vm = 0; }
				if (_vm && _vname) {
					add_next_index_stringl(&arr, _vname, _vname_len);
				} else {
					add_next_index_null(&arr);
				}
			}
			add_assoc_zval_ex(dst, ("vars"), (sizeof("vars")), &arr);
		} else {
			add_assoc_null_ex(dst, ("vars"), (sizeof("vars")));
		}
	}
#else
	if (src->vars && src->last_var > 0 && src->last_var < 65536) {
		int i;
		zval arr;
		array_init(&arr);
		for (i = 0; i < src->last_var; ++i) {
			add_next_index_string(&arr, ZSTR_VAL(src->vars[i]));
		}
		add_assoc_zval_ex(dst, ("vars"), (sizeof("vars")), &arr);
	} else {
		add_assoc_null_ex(dst, ("vars"), (sizeof("vars")));
	}
#endif

	add_assoc_long_ex(dst, ("last_var"), (sizeof("last_var")), src->last_var);

	if (src->prototype
#ifdef PHP_WIN32
	    && dasm_ic_committed_readable_ptr(src->prototype)
#endif
	) {
		zval zv;
		zend_op_array *op_array;
		array_init(&zv);
#ifdef PHP_WIN32
		__try {
			add_assoc_long(&zv, "type", src->prototype->type);
			op_array = &(src->prototype->op_array);
			dasm_zend_op_array(&zv, op_array);
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			add_assoc_null(&zv, "type");
		}
#else
		add_assoc_long(&zv, "type", src->prototype->type);
		op_array = &(src->prototype->op_array);
		dasm_zend_op_array(&zv, op_array);
#endif
		add_assoc_zval_ex(dst, ("prototype"), (sizeof("prototype")), &zv);
	} else {
		add_assoc_null_ex(dst, ("prototype"), (sizeof("prototype")));
	}

	if (src->scope) {
		const char *_scope = NULL;
		size_t _scope_len = 0;
#ifdef PHP_WIN32
		__try {
			if (dasm_ic_committed_readable_ptr(src->scope) &&
			    dasm_ic_committed_readable_ptr(src->scope->name)) {
				_scope = ZSTR_VAL(src->scope->name);
				_scope_len = ZSTR_LEN(src->scope->name);
				if (_scope_len > 32768 ||
				    !dasm_ic_committed_readable_ptr(_scope) ||
				    !dasm_ic_committed_readable_ptr(_scope + _scope_len)) {
					_scope = NULL;
					_scope_len = 0;
				}
			}
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			_scope = NULL;
			_scope_len = 0;
		}
#else
		_scope = ZSTR_VAL(src->scope->name);
		_scope_len = ZSTR_LEN(src->scope->name);
#endif
		if (_scope) {
			add_assoc_stringl(dst, "scope", (char *)_scope, _scope_len);
		} else {
			add_assoc_null_ex(dst, ("scope"), (sizeof("scope")));
		}
	} else {
		add_assoc_null_ex(dst, ("scope"), (sizeof("scope")));
	}

	dasm_lambda_closures(dst, src);
}

/* ============================================================
 * ionCube force-decode  (Windows only)
 * ============================================================
 * PHP 7.2 NTS loader (ioncube_loader_win_7.2.dll) — confirmed by IDA:
 *
 *  sub_100636A0 (RINIT):   dword_100BEEA8 = time32(0)  [the XOR key]
 *  sub_100627B0 (encode):  thiscall(op_array)
 *    - saves op_array->opcodes (byte 40) XOR-encoded into descriptor[5]
 *    - replaces opcodes with a 28-byte sentinel from descriptor[6]
 *    - saves last (byte 36) into descriptor[27], zeros it
 *    - ORs 0x400000 into line_end (byte 80)
 *  sub_10002C30 = step1:   thiscall(op_array)
 *    - zeros opcodes (byte 40)
 *    - calls the runtime decrypt callback; unavailable when descriptor[19] is NULL
 *  sub_100628D0 = step2:   thiscall(op_array)
 *    - XOR-decodes descriptor[5] → restores original T (byte 40)
 *      key = dword_100BEEA8 + op_array->line_start (byte 76) + descriptor[17]
 *    - restores last (byte 36) from descriptor[27]
 *    - clears 0x400000 from line_end (byte 80)
 *
 *  Encode marker (same guard as step2):
 *    - op_array->reserved[3] != NULL  (descriptor present)
 *    - *(uint32_t*)((char*)oa + 80) & 0x400000  (bit in line_end)
 *  NOT opcodes&3 — that was PHP 7.1 behaviour.
 */
#ifdef PHP_WIN32
#ifndef IC_STEP1_RVA
#define IC_STEP1_RVA  0x2C30u
#endif
#ifndef IC_STEP2_RVA
#define IC_STEP2_RVA  0x628D0u
#endif
#ifndef IC_REQUEST_KEY_RVA
#define IC_REQUEST_KEY_RVA 0xBEEA8u
#endif
static void ic_decode_const_operand_literal(zend_op_array *oa, znode_op operand,
                                             uint8_t *decoded_literals, uint32_t xor_key)
{
	zend_long literal_index;
	zval *literal;

	if (oa == NULL || decoded_literals == NULL || operand.zv == NULL) return;
	literal_index = dasm_literal_index(oa, operand.zv);
	if (literal_index < 0 || decoded_literals[literal_index]) return;
	literal = &oa->literals[literal_index];
	if (Z_TYPE_P(literal) != IS_LONG) return;
	__try {
		Z_LVAL_P(literal) = (zend_long)((uint32_t)Z_LVAL_P(literal) ^ xor_key);
		decoded_literals[literal_index] = 1;
	} __except(EXCEPTION_EXECUTE_HANDLER) {}
}

static void ic_decode_marked_const_literals(zend_op_array *oa)
{
	uint8_t *decoded_literals;
	zend_long i;
	uint32_t xor_key;
	DWORD old_protect = 0;
	int unlocked = 0;

	if (oa == NULL || oa->opcodes == NULL || oa->literals == NULL ||
	    oa->last <= 0 || oa->last_literal <= 0 ||
	    oa->last >= 65536 || oa->last_literal >= 65536) return;

	decoded_literals = (uint8_t *)ecalloc((size_t)oa->last_literal, sizeof(uint8_t));
	if (decoded_literals == NULL) return;

	xor_key = (uint32_t)oa->last_literal + 1u;

	unlocked = VirtualProtect((LPVOID)oa->literals,
	    (SIZE_T)oa->last_literal * sizeof(zval), PAGE_EXECUTE_READWRITE, &old_protect);
	if (!unlocked)
		unlocked = VirtualProtect((LPVOID)oa->literals,
		    (SIZE_T)oa->last_literal * sizeof(zval), PAGE_READWRITE, &old_protect);

	for (i = 0; i < (zend_long)oa->last; ++i) {
		zend_op *opline = &oa->opcodes[i];
		if (opline->op1_type == IS_CV &&
		    opline->op2_type == IS_CONST &&
		    opline->result_type == IS_UNUSED) {
			ic_decode_const_operand_literal(oa, opline->op2, decoded_literals, xor_key);
		}
	}

	if (unlocked)
		VirtualProtect((LPVOID)oa->literals,
		    (SIZE_T)oa->last_literal * sizeof(zval), old_protect, &old_protect);
	efree(decoded_literals);
}

static zend_op *ic_manual_restore_step2_opcodes(zend_op_array *oa, uintptr_t loader_base, void *ic_desc)
{
	const uint32_t *desc;
	uint32_t request_key;
	uint32_t raw_line_start;
	uint32_t key;
	uint32_t decoded_opcodes;
	uint32_t delta_count;

	if (oa == NULL || loader_base == 0 || ic_desc == NULL) return NULL;
	if (!dasm_ic_committed_readable_ptr(ic_desc) ||
	    !dasm_ic_committed_readable_ptr((const char *)ic_desc + 0x6c) ||
	    !dasm_ic_committed_readable_ptr((const void *)(loader_base + IC_REQUEST_KEY_RVA))) {
		return NULL;
	}

	desc = (const uint32_t *)ic_desc;
	__try {
		request_key = *(const uint32_t *)(loader_base + IC_REQUEST_KEY_RVA);
		raw_line_start = *(const uint32_t *)((char *)oa + 76);
		key = request_key + raw_line_start + desc[17];
		decoded_opcodes = desc[5] ^ key;

		if (decoded_opcodes == 0) {
			return NULL;
		}

		oa->opcodes = (zend_op *)(uintptr_t)decoded_opcodes;
		oa->last = desc[27];
		*(uint32_t *)((char *)oa + 80) &= ~0x400000u;

		delta_count = (desc[5] - desc[16]) / sizeof(zend_op);
		__try {
			((uint32_t *)ic_desc)[15] = decoded_opcodes - (uint32_t)(sizeof(zend_op) * delta_count);
		} __except(EXCEPTION_EXECUTE_HANDLER) {}
		return (zend_op *)(uintptr_t)decoded_opcodes;
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		return NULL;
	}
}

static void ic_force_decode_op_array(zend_op_array *oa, uintptr_t loader_base)
{
	if (!oa) return;
	/* PHP 7.2 NTS ionCube encode marker (mirrors step2 guard):
	 *   reserved[3] holds the descriptor pointer (set by sub_100627B0)
	 *   bit 0x400000 is ORed into try_catch_array (byte offset 80 = 0x50) */
	void *ic_desc = oa->reserved[3];
	uint32_t ic_flags = *(uint32_t *)((char *)oa + 80);   /* try_catch_array raw bits */
	int has_step2_marker = (ic_flags & 0x400000u) != 0;
	int has_odd_opcode_sentinel = (oa->opcodes && (((uintptr_t)oa->opcodes) & 3)) != 0;
	if (!ic_desc || (!has_step2_marker && !has_odd_opcode_sentinel)) return;
	if (!dasm_ic_committed_readable_ptr(ic_desc) ||
	    !dasm_ic_committed_readable_ptr((const char *)ic_desc + 0x6c)) {
		return;
	}

	typedef int (__fastcall *fn_t)(void *ecx_this);
	fn_t step1 = (fn_t)(loader_base + IC_STEP1_RVA);
	fn_t step2 = (fn_t)(loader_base + IC_STEP2_RVA);
	zend_op *decoded_ops = NULL;
	zend_op *restored_ops = NULL;
	uint32_t saved_last = oa->last;
	int can_call_step1 = 0;

	/* descriptor[19] is NULL for some op_arrays before runtime dispatch.
	 * step1 dereferences it unconditionally, so use it only as a protected
	 * gate for step1. */
	__try {
		const uint32_t *desc = (const uint32_t *)ic_desc;
		uint32_t fn_ptr_val = desc[19];
		can_call_step1 = (fn_ptr_val != 0);
		if (saved_last == 0) saved_last = desc[27];
		if (saved_last == 0 && fn_ptr_val) {
			const uint32_t *fn_ptr = (const uint32_t *)fn_ptr_val;
			const zend_op_array *cand = (const zend_op_array *)(uintptr_t)fn_ptr[10];
			if (dasm_ic_committed_readable_ptr(cand) &&
			    cand->last > 0 && cand->last < 65536) {
				saved_last = cand->last;
			}
		}
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		can_call_step1 = 0;
	}

	if (can_call_step1) {
		__try { step1(oa); } __except(EXCEPTION_EXECUTE_HANDLER) {}

		/* Byte 40 (offset 0x28) holds the real decoded opline ptr written
		 * by step1's decrypt callback.  Capture it BEFORE step2 runs,
		 * because step2 restores the encoded field (mirrors dispatch at
		 * ioncube_loader+0x629ec: mov ecx,[edi+28h]; mov [esi],ecx). */
		__try {
			decoded_ops = *(zend_op **)((char *)oa + 40);
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			decoded_ops = NULL;
		}
	}

	if (has_step2_marker) {
		__try { step2(oa); } __except(EXCEPTION_EXECUTE_HANDLER) {}
	}

	/* Some compile-time op_arrays have descriptor[19] == NULL, so step1
	 * cannot run.  step2 may also be unsafe in that state; reproduce its
	 * pointer restore directly from descriptor[5] and the request key. */
	if (ic_desc) {
		uint32_t still_marked = 0;
		uint32_t stub_ops = 0;
		__try {
			const uint32_t *desc = (const uint32_t *)ic_desc;
			still_marked = (*(uint32_t *)((char *)oa + 80) & 0x400000u) != 0;
			stub_ops = desc[6];
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			still_marked = 0;
			stub_ops = 0;
		}
		if (still_marked || (stub_ops && (uintptr_t)oa->opcodes == (uintptr_t)stub_ops)) {
			restored_ops = ic_manual_restore_step2_opcodes(oa, loader_base, ic_desc);
		}
	}

	/* After step1, byte 40 has the dispatch opline pointer.  For op_arrays
	 * without runtime state, use the descriptor-based step2 fallback instead. */
	if (decoded_ops) oa->opcodes = decoded_ops;
	else if (restored_ops) oa->opcodes = restored_ops;
	if (oa->last == 0) {
		if (saved_last > 0 && saved_last < 65536) oa->last = saved_last;
	}
	if (oa->last == 0 && ic_desc) {
		__try {
			const uint32_t *desc = (const uint32_t *)ic_desc;
			if (desc[27] > 0 && desc[27] < 65536) oa->last = desc[27];
		} __except(EXCEPTION_EXECUTE_HANDLER) {}
	}

	__try { ic_decode_marked_const_literals(oa); } __except(EXCEPTION_EXECUTE_HANDLER) {}

	if (!oa->reserved[2] && ic_desc) {
		__try {
			const uint32_t *desc = (const uint32_t *)ic_desc;
			if (desc[6]) oa->reserved[2] = (void *)(uintptr_t)desc[6];
		} __except(EXCEPTION_EXECUTE_HANDLER) {}
	}
	if (ic_desc && !oa->reserved[3]) oa->reserved[3] = ic_desc;
}

static void ic_force_decode_all(uintptr_t loader_base)
{
	zend_function *zif;
	zend_class_entry *ce;

	if (!loader_base) return;

	if (CG(function_table)) {
		ZEND_HASH_FOREACH_PTR(CG(function_table), zif) {
			if (zif->type == ZEND_USER_FUNCTION)
				ic_force_decode_op_array(&zif->op_array, loader_base);
		} ZEND_HASH_FOREACH_END();
	}
	if (CG(class_table)) {
		ZEND_HASH_FOREACH_PTR(CG(class_table), ce) {
			if (ce->type == ZEND_USER_CLASS) {
				ZEND_HASH_FOREACH_PTR(&ce->function_table, zif) {
					if (zif->type == ZEND_USER_FUNCTION)
						ic_force_decode_op_array(&zif->op_array, loader_base);
				} ZEND_HASH_FOREACH_END();
			}
		} ZEND_HASH_FOREACH_END();
	}
}
#endif /* PHP_WIN32 */

/* ============================================================
 * PHP functions
 * ============================================================ */
#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_dasm_file, 0, 0, 1)
	ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_dasm_file[] = { 1, BYREF_NONE };
#endif

PHP_FUNCTION(dasm_file)
{
	char *filename;
	size_t filename_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &filename, &filename_len) == FAILURE) {
		return;
	}
	if (!filename_len) RETVAL_FALSE;

	zend_op_array *op_array = NULL;
	zval zfilename;
#ifdef PHP_WIN32
	uintptr_t loader_base = 0;
	{
		HMODULE hLoader = GetModuleHandleA(IC_LOADER_NAME);
		if (hLoader) {
			loader_base = (uintptr_t)hLoader;
		}
	}
#endif
	ZVAL_STRING(&zfilename, filename);
	op_array = compile_filename(ZEND_REQUIRE, &zfilename);

#ifdef PHP_WIN32
	{
		if (loader_base) {
			ic_force_decode_all(loader_base);
			if (op_array) ic_force_decode_op_array(op_array, loader_base);
		}
	}
#endif

	if (op_array) {
		op_array->line_start &= ~0x600000u;
		op_array->line_end   &= ~0x600000u;
	}

	array_init(return_value);
#ifdef PHP_WIN32
#endif

	zval zv;
	array_init(&zv);
	dasm_zend_op_array(&zv, op_array);
	add_assoc_string(return_value, "filename", filename);
	add_assoc_zval_ex(return_value, ZEND_STRS("op_array"), &zv);

	if (CG(function_table)) {
		zval _zv;
		array_init(&_zv);
		dasm_function_table(&_zv, CG(function_table));
		add_assoc_zval_ex(return_value, ("function_table"), (sizeof("function_table")), &_zv);
	} else if (EG(function_table)) {
		zval _zv;
		array_init(&_zv);
		dasm_function_table(&_zv, EG(function_table));
		add_assoc_zval_ex(return_value, ("function_table"), (sizeof("function_table")), &_zv);
	} else {
		add_assoc_null_ex(return_value, ("function_table"), (sizeof("function_table")));
	}

	if (CG(class_table)) {
		zval _zv;
		array_init(&_zv);
		dasm_class_table(&_zv, CG(class_table));
		add_assoc_zval_ex(return_value, ("class_table"), (sizeof("class_table")), &_zv);
	} else if (EG(class_table)) {
		zval _zv;
		array_init(&_zv);
		dasm_class_table(&_zv, EG(class_table));
		add_assoc_zval_ex(return_value, ("class_table"), (sizeof("class_table")), &_zv);
	} else {
		add_assoc_null_ex(return_value, ("class_table"), (sizeof("class_table")));
	}

	zval_ptr_dtor(&zfilename);
}

#ifdef ZEND_BEGIN_ARG_INFO_EX
ZEND_BEGIN_ARG_INFO_EX(arginfo_dasm_string, 0, 0, 1)
	ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()
#else
static unsigned char arginfo_dasm_string[] = { 1, BYREF_NONE };
#endif

PHP_FUNCTION(dasm_string)
{
	zval *code;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &code) == FAILURE) {
		return;
	}

	zend_op_array *op_array = NULL;
	char *eval_name = zend_make_compiled_string_description("runtime-created function" TSRMLS_CC);
	op_array = compile_string(code, eval_name TSRMLS_CC);

	array_init(return_value);
	zval zv;
	array_init(&zv);
	dasm_zend_op_array(&zv, op_array);
	add_assoc_zval_ex(return_value, ZEND_STRS("op_array"), &zv);

	if (CG(function_table)) {
		zval _zv;
		array_init(&_zv);
		dasm_function_table(&_zv, CG(function_table));
		add_assoc_zval_ex(return_value, ("function_table"), (sizeof("function_table")), &_zv);
	} else {
		add_assoc_null_ex(return_value, ("function_table"), (sizeof("function_table")));
	}
	if (CG(class_table)) {
		zval _zv;
		array_init(&_zv);
		dasm_class_table(&_zv, CG(class_table));
		add_assoc_zval_ex(return_value, ("class_table"), (sizeof("class_table")), &_zv);
	} else {
		add_assoc_null_ex(return_value, ("class_table"), (sizeof("class_table")));
	}
}

PHP_MINIT_FUNCTION(opcodedump)    { return SUCCESS; }
PHP_MSHUTDOWN_FUNCTION(opcodedump){ return SUCCESS; }

PHP_RINIT_FUNCTION(opcodedump)
{
#if defined(COMPILE_DL_OPCODEDUMP) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(opcodedump){ return SUCCESS; }

PHP_MINFO_FUNCTION(opcodedump)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "opcodedump support", "enabled");
	php_info_print_table_end();
}

const zend_function_entry opcodedump_functions[] = {
	PHP_FE(dasm_file,   arginfo_dasm_file)
	PHP_FE(dasm_string, arginfo_dasm_string)
	PHP_FE_END
};

zend_module_entry opcodedump_module_entry = {
	STANDARD_MODULE_HEADER,
	"opcodedump",
	opcodedump_functions,
	PHP_MINIT(opcodedump),
	PHP_MSHUTDOWN(opcodedump),
	PHP_RINIT(opcodedump),
	PHP_RSHUTDOWN(opcodedump),
	PHP_MINFO(opcodedump),
	PHP_OPCODEDUMP_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_OPCODEDUMP
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(opcodedump)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
