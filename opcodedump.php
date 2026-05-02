<?php

if (!extension_loaded('opcodedump')) {
    fwrite(STDERR, "Extension 'opcodedump' is not loaded.\n");
    exit(1);
}

function normalize_key($key)
{
    static $aliases = array(
        'function_nam' => 'function_name',
        'function_tabl' => 'function_table',
        'class_tabl' => 'class_table',
        'arg_inf' => 'arg_info',
        'doc_commen' => 'doc_comment',
        'doc_comment_le' => 'doc_comment_len',
        'op_arra' => 'op_array',
        'typ' => 'type',
        'name_le' => 'name_len',
        'class_nam' => 'class_name',
        'class_name_le' => 'class_name_len',
        'default_static_members_tabl' => 'default_static_members_table',
        'properties_inf' => 'properties_info',
        'offse' => 'offset',
        'flag' => 'flags',
        'op1.litera' => 'op1.literal',
        'op2.litera' => 'op2.literal',
        'result.litera' => 'result.literal',
        'handle' => 'handler',
        'opcode_inde' => 'opcode_index',
        'opcod' => 'opcode',
        'literal_inde' => 'literal_index',
        'parent_scop' => 'parent_scope',
        'closure_repor' => 'closure_report',
        'declared_lambda' => 'declared_lambdas',
        'dumped_closure_op_array' => 'dumped_closure_op_arrays',
        'missin' => 'missing',
    );

    if (!is_string($key)) {
        return $key;
    }

    $key = str_replace("\0", '', $key);

    if (isset($aliases[$key])) {
        return $aliases[$key];
    }

    return $key;
}

function normalize_dump($value)
{
    if (!is_array($value)) {
        return $value;
    }

    $normalized = array();
    foreach ($value as $key => $item) {
        $key = normalize_key($key);
        $normalized[$key] = normalize_dump($item);
    }

    return $normalized;
}

function reorder_assoc(array $value, array $orderedKeys)
{
    $reordered = array();

    foreach ($orderedKeys as $key) {
        if (array_key_exists($key, $value)) {
            $reordered[$key] = $value[$key];
            unset($value[$key]);
        }
    }

    foreach ($value as $key => $item) {
        $reordered[$key] = $item;
    }

    return $reordered;
}

function normalize_opcode_entry(array $opcode)
{
    return reorder_assoc($opcode, array(
        'opcode',
        'op1_type',
        'op2_type',
        'result_type',
        'extended_value',
        'lineno',
        'opcode_name',
        'op1_type_name',
        'op2_type_name',
        'result_type_name',
        'op1.literal',
        'op2.literal',
        'result.literal',
        'op1.cv_name',
        'op2.cv_name',
        'result.cv_name',
        'result.constant',
        'result.var',
        'result.num',
        'result.opline_num',
        'op1.constant',
        'op1.var',
        'op1.num',
        'op1.opline_num',
        'op2.constant',
        'op2.var',
        'op2.num',
        'op2.opline_num',
        'handler',
    ));
}

function normalize_arg_info_entry(array $argInfo)
{
    return reorder_assoc($argInfo, array(
        'name',
        'name_len',
        'class_name',
        'class_name_len',
        'type',
        'type_hint',
        'allow_null',
        'pass_by_reference',
        'is_variadic',
    ));
}

function normalize_property_info_entry(array $propertyInfo)
{
    return reorder_assoc($propertyInfo, array(
        'name',
        'name_len',
        'doc_comment',
        'doc_comment_len',
        'offset',
        'flags',
        'ce',
    ));
}

function normalize_op_array_entry(array $opArray)
{
    if (isset($opArray['arg_info']) && is_array($opArray['arg_info'])) {
        foreach ($opArray['arg_info'] as $index => $argInfo) {
            if (is_array($argInfo)) {
                $opArray['arg_info'][$index] = normalize_arg_info_entry($argInfo);
            }
        }
    }

    if (isset($opArray['opcodes']) && is_array($opArray['opcodes'])) {
        foreach ($opArray['opcodes'] as $index => $opcode) {
            if (is_array($opcode)) {
                $opArray['opcodes'][$index] = normalize_opcode_entry($opcode);
            }
        }
    }

    return reorder_assoc($opArray, array(
        'type',
        'function_name',
        'fn_flags',
        'arg_info',
        'num_args',
        'required_num_args',
        'refcount',
        'literals',
        'last_literal',
        'opcodes',
        'last',
        'T',
        'live_range',
        'last_live_range',
        'try_catch_array',
        'last_try_catch',
        'static_variables',
        'closures',
        'closure_report',
        'filename',
        'doc_comment',
        'doc_comment_len',
        'line_start',
        'line_end',
        'early_binding',
        'cache_size',
        'vars',
        'last_var',
        'prototype',
        'scope',
    ));
}

function normalize_function_table(array $functionTable, $sourcePath)
{
    $normalized = array();
    $normalizedSource = normalize_path($sourcePath);

    foreach ($functionTable as $name => $entry) {
        if (!is_array($entry) || !isset($entry['op_array']) || !is_array($entry['op_array'])) {
            continue;
        }

        $opArray = $entry['op_array'];
        $entryPath = isset($opArray['filename']) ? normalize_path($opArray['filename']) : null;
        if ($entryPath !== $normalizedSource && $entryPath !== normalize_path('(ionCube-protected)')) {
            continue;
        }

        $entry['op_array'] = normalize_op_array_entry($opArray);
        $normalized[$name] = reorder_assoc($entry, array('op_array'));
    }

    return $normalized;
}

function normalize_class_table(array $classTable, $sourcePath)
{
    $normalized = array();

    foreach ($classTable as $name => $classInfo) {
        if (!is_array($classInfo)) {
            continue;
        }

        if (isset($classInfo['function_table']) && is_array($classInfo['function_table'])) {
            $classInfo['function_table'] = normalize_function_table($classInfo['function_table'], $sourcePath);
        }

        if (isset($classInfo['properties_info']) && is_array($classInfo['properties_info'])) {
            foreach ($classInfo['properties_info'] as $propName => $propInfo) {
                if (is_array($propInfo)) {
                    $classInfo['properties_info'][$propName] = normalize_property_info_entry($propInfo);
                }
            }
        }

        $normalized[$name] = reorder_assoc($classInfo, array(
            'type',
            'name',
            'name_len',
            'parent',
            'refcount',
            'ce_flags',
            'default_properties_count',
            'default_static_members_count',
            'default_properties_table',
            'default_static_members_table',
            'function_table',
            'properties_info',
            'constants_table',
            'interfaces',
            'traits',
        ));
    }

    return $normalized;
}

function prepare_dump_for_output(array $dump, $sourcePath)
{
    $prepared = $dump;

    $prepared['filename'] = basename($sourcePath);

    if (isset($prepared['op_array']) && is_array($prepared['op_array'])) {
        $prepared['op_array'] = normalize_op_array_entry($prepared['op_array']);
    }

    if (isset($prepared['function_table']) && is_array($prepared['function_table'])) {
        $prepared['function_table'] = normalize_function_table($prepared['function_table'], $sourcePath);
    }

    if (isset($prepared['class_table']) && is_array($prepared['class_table'])) {
        $prepared['class_table'] = normalize_class_table($prepared['class_table'], $sourcePath);
    }

    return reorder_assoc($prepared, array(
        'filename',
        'op_array',
        'function_table',
        'class_table',
    ));
}

function resolve_target($path, $scriptDir)
{
    $candidates = array(
        $path,
        $scriptDir . DIRECTORY_SEPARATOR . $path,
        getcwd() . DIRECTORY_SEPARATOR . $path,
    );

    foreach ($candidates as $candidate) {
        $real = realpath($candidate);
        if ($real !== false && is_file($real)) {
            return $real;
        }
    }

    return false;
}

function dump_output_path($path)
{
    return preg_replace('/\.php$/i', '', $path) . '.opcodes.txt';
}

function dump_json_output_path($path)
{
    return preg_replace('/\.php$/i', '', $path) . '.opcodes.json';
}

function opcode_label($opcode)
{
    if (isset($opcode['opcode_name']) && $opcode['opcode_name'] !== '') {
        return $opcode['opcode_name'];
    }

    return '#' . (isset($opcode['opcode']) ? $opcode['opcode'] : '?');
}

function normalize_path($path)
{
    if (!is_string($path)) {
        return null;
    }

    $path = str_replace('/', DIRECTORY_SEPARATOR, $path);

    return strtolower($path);
}

function describe_value($value)
{
    if (is_null($value)) {
        return null;
    }

    if (is_bool($value) || is_int($value) || is_float($value) || is_string($value)) {
        return $value;
    }

    if (is_array($value)) {
        $described = array();
        $count = 0;
        foreach ($value as $key => $item) {
            $described[$key] = describe_value($item);
            $count++;
            if ($count >= 20) {
                $described['...'] = 'truncated';
                break;
            }
        }
        return $described;
    }

    if (is_object($value)) {
        return 'object(' . get_class($value) . ')';
    }

    if (is_resource($value)) {
        return 'resource';
    }

    return (string) $value;
}

function is_printable_string($value)
{
    return is_string($value) && preg_match('/\A[\x09\x0A\x0D\x20-\x7E]*\z/', $value);
}

function safe_string($value, $previewLength = 160)
{
    $length = strlen($value);
    $preview = substr($value, 0, $previewLength);

    return array(
        'type' => 'string',
        'length' => $length,
        'printable' => is_printable_string($value),
        'value' => is_printable_string($value) ? $value : null,
        'preview' => is_printable_string($preview) ? $preview : null,
        'hex' => bin2hex($value),
        'base64' => base64_encode($value),
        'sha1' => sha1($value),
    );
}

function safe_value($value)
{
    if (is_string($value)) {
        return safe_string($value);
    }
    if (is_null($value)) {
        return array('type' => 'null', 'value' => null);
    }
    if (is_bool($value)) {
        return array('type' => 'bool', 'value' => $value);
    }
    if (is_int($value)) {
        return array('type' => 'int', 'value' => $value);
    }
    if (is_float($value)) {
        return array('type' => 'float', 'value' => $value);
    }
    if (is_array($value)) {
        $items = array();
        foreach ($value as $key => $item) {
            $items[$key] = safe_value($item);
        }
        return array('type' => 'array', 'value' => $items);
    }

    return array('type' => gettype($value), 'value' => (string)$value);
}

function literal_ir($literal, $index)
{
    $safe = safe_value($literal);
    $safe['index'] = $index;
    return reorder_assoc($safe, array(
        'index',
        'type',
        'length',
        'printable',
        'value',
        'preview',
        'hex',
        'base64',
        'sha1',
    ));
}

function operand_ir(array $opcode, $prefix)
{
    $typeKey = $prefix . '_type';
    $typeNameKey = $prefix . '_type_name';
    $literalKey = $prefix . '.literal';
    $cvNameKey = $prefix . '.cv_name';
    $constantKey = $prefix . '.constant';
    $varKey = $prefix . '.var';
    $numKey = $prefix . '.num';
    $oplineKey = $prefix . '.opline_num';

    $operand = array(
        'type' => isset($opcode[$typeKey]) ? $opcode[$typeKey] : null,
        'type_name' => isset($opcode[$typeNameKey]) ? $opcode[$typeNameKey] : null,
        'constant' => isset($opcode[$constantKey]) ? $opcode[$constantKey] : null,
        'var' => isset($opcode[$varKey]) ? $opcode[$varKey] : null,
        'num' => isset($opcode[$numKey]) ? $opcode[$numKey] : null,
        'opline_num' => isset($opcode[$oplineKey]) ? $opcode[$oplineKey] : null,
        'cv_name' => isset($opcode[$cvNameKey]) ? $opcode[$cvNameKey] : null,
    );

    if (array_key_exists($literalKey, $opcode)) {
        $operand['literal'] = safe_value($opcode[$literalKey]);
    }

    return $operand;
}

function opcode_jump_targets(array $opcode)
{
    $name = isset($opcode['opcode_name']) ? $opcode['opcode_name'] : '';
    $targets = array();

    $op1Target = array('ZEND_JMP', 'ZEND_FAST_CALL');
    $op2Target = array(
        'ZEND_JMPZ',
        'ZEND_JMPNZ',
        'ZEND_JMPZNZ',
        'ZEND_JMPZ_EX',
        'ZEND_JMPNZ_EX',
        'ZEND_FE_RESET_R',
        'ZEND_FE_RESET_RW',
        'ZEND_FE_FETCH_R',
        'ZEND_FE_FETCH_RW',
        'ZEND_JMP_SET',
        'ZEND_COALESCE',
        'ZEND_ASSERT_CHECK',
        'ZEND_CATCH',
    );

    if (in_array($name, $op1Target, true) && isset($opcode['op1.opline_num'])) {
        $targets[] = (int)$opcode['op1.opline_num'];
    }
    if (in_array($name, $op2Target, true) && isset($opcode['op2.opline_num'])) {
        $targets[] = (int)$opcode['op2.opline_num'];
    }
    // JMPZNZ second target: non-zero/non-null branch resolved by C extension
    if ($name === 'ZEND_JMPZNZ' && isset($opcode['jmpznz_true_opline']) && (int)$opcode['jmpznz_true_opline'] >= 0) {
        $targets[] = (int)$opcode['jmpznz_true_opline'];
    }

    return array_values(array_unique(array_filter($targets, function ($target) {
        return $target >= 0;
    })));
}

// PHP 7.2 extended_value semantics per opcode (zend_compile.h)
function decode_extended_value($opcodeName, $extVal)
{
    if ($extVal === null) {
        return null;
    }

    switch ($opcodeName) {
        case 'ZEND_INCLUDE_OR_EVAL':
            // ZEND_EVAL=1 ZEND_INCLUDE=2 ZEND_INCLUDE_ONCE=4 ZEND_REQUIRE=8 ZEND_REQUIRE_ONCE=16
            $map = array(1 => 'eval', 2 => 'include', 4 => 'include_once', 8 => 'require', 16 => 'require_once');
            return isset($map[$extVal]) ? $map[$extVal] : 'unknown(' . $extVal . ')';

        case 'ZEND_CAST':
            // IS_NULL=1 IS_LONG=4 IS_DOUBLE=5 IS_STRING=6 IS_ARRAY=7 IS_OBJECT=8 _IS_BOOL=13
            $map = array(1 => 'null', 2 => 'false', 3 => 'bool', 4 => 'int', 5 => 'float',
                         6 => 'string', 7 => 'array', 8 => 'object', 13 => 'bool');
            return isset($map[$extVal]) ? $map[$extVal] : 'unknown(' . $extVal . ')';

        case 'ZEND_CATCH':
            // PHP 7.2: extended_value != 0 means this is the last catch in the chain
            return array('is_last_catch' => (bool)$extVal);

        case 'ZEND_FETCH_R':
        case 'ZEND_FETCH_W':
        case 'ZEND_FETCH_RW':
        case 'ZEND_FETCH_FUNC_ARG':
        case 'ZEND_FETCH_UNSET':
        case 'ZEND_FETCH_IS':
            // ZEND_FETCH_TYPE_MASK=0x70000000 ZEND_FETCH_GLOBAL=0 ZEND_FETCH_LOCAL=0x10000000
            $type = $extVal & 0x70000000;
            $typeMap = array(0 => 'global', 0x10000000 => 'local', 0x40000000 => 'global_lock');
            return array(
                'fetch_type' => isset($typeMap[$type]) ? $typeMap[$type] : 'unknown(0x' . dechex($type) . ')',
                'arg'        => $extVal & 0x000fffff,
            );

        default:
            return null;
    }
}

function opcode_ir(array $opcode, $index)
{
    $targets    = opcode_jump_targets($opcode);
    $opcodeName = opcode_label($opcode);
    $extVal     = isset($opcode['extended_value']) ? $opcode['extended_value'] : null;

    // Mask ionCube-injected bits from lineno (same mask used for line_start/line_end in C ext)
    $linenoRaw = isset($opcode['lineno']) ? (int)$opcode['lineno'] : null;
    $lineno    = $linenoRaw !== null ? ($linenoRaw & ~0x600000) : null;

    $ir = array(
        'index'                  => $index,
        'line'                   => $lineno,
        'lineno_raw'             => $linenoRaw !== $lineno ? $linenoRaw : null,
        'opcode'                 => isset($opcode['opcode']) ? $opcode['opcode'] : null,
        'opcode_name'            => $opcodeName,
        'extended_value'         => $extVal,
        'extended_value_decoded' => decode_extended_value($opcodeName, $extVal),
        'op1'                    => operand_ir($opcode, 'op1'),
        'op2'                    => operand_ir($opcode, 'op2'),
        'result'                 => operand_ir($opcode, 'result'),
        'jump_targets'           => $targets,
        'is_call'                => in_array($opcodeName, array('ZEND_INIT_FCALL', 'ZEND_INIT_FCALL_BY_NAME', 'ZEND_DO_FCALL', 'ZEND_DO_FCALL_BY_NAME'), true),
        'is_include_or_eval'     => $opcodeName === 'ZEND_INCLUDE_OR_EVAL',
        'is_lambda_declare'      => $opcodeName === 'ZEND_DECLARE_LAMBDA_FUNCTION',
    );

    // JMPZNZ second target from C extension (non-zero/non-null branch)
    if ($opcodeName === 'ZEND_JMPZNZ' && isset($opcode['jmpznz_true_opline'])) {
        $ir['jmpznz_true_opline'] = (int)$opcode['jmpznz_true_opline'];
    }

    // Propagate decode_failed flag set by C extension on unreadable opcodes
    if (!empty($opcode['decode_failed'])) {
        $ir['decode_failed'] = true;
    }

    return $ir;
}

function opcode_falls_through($opcodeName)
{
    return !in_array($opcodeName, array(
        'ZEND_JMP',
        'ZEND_RETURN',
        'ZEND_THROW',
        'ZEND_EXIT',
    ), true);
}

function build_cfg(array $opcodes)
{
    $count = count($opcodes);
    if ($count === 0) {
        return array('blocks' => array(), 'edges' => array());
    }

    $leaders = array(0 => true);
    foreach ($opcodes as $index => $opcode) {
        foreach (opcode_jump_targets($opcode) as $target) {
            if ($target >= 0 && $target < $count) {
                $leaders[$target] = true;
            }
        }
        if (!empty(opcode_jump_targets($opcode)) && $index + 1 < $count) {
            $leaders[$index + 1] = true;
        }
    }

    $leaderList = array_keys($leaders);
    sort($leaderList, SORT_NUMERIC);

    $blocks = array();
    $opToBlock = array();
    foreach ($leaderList as $blockIndex => $start) {
        $end = ($blockIndex + 1 < count($leaderList)) ? $leaderList[$blockIndex + 1] - 1 : $count - 1;
        $id = 'B' . $blockIndex;
        $blocks[] = array(
            'id' => $id,
            'start' => $start,
            'end' => $end,
        );
        for ($i = $start; $i <= $end; $i++) {
            $opToBlock[$i] = $id;
        }
    }

    $edges = array();
    foreach ($blocks as $block) {
        $lastIndex = $block['end'];
        $lastOpcode = isset($opcodes[$lastIndex]) && is_array($opcodes[$lastIndex]) ? $opcodes[$lastIndex] : array();
        $lastName = opcode_label($lastOpcode);

        foreach (opcode_jump_targets($lastOpcode) as $target) {
            if (isset($opToBlock[$target])) {
                $edges[] = array('from' => $block['id'], 'to' => $opToBlock[$target], 'kind' => 'jump');
            }
        }

        if (opcode_falls_through($lastName) && $lastIndex + 1 < $count && isset($opToBlock[$lastIndex + 1])) {
            $edges[] = array('from' => $block['id'], 'to' => $opToBlock[$lastIndex + 1], 'kind' => 'fallthrough');
        }
    }

    return array('blocks' => $blocks, 'edges' => $edges);
}

function build_opcode_stats(array $opcodes)
{
    $stats = array();
    foreach ($opcodes as $opcode) {
        if (!is_array($opcode)) {
            continue;
        }
        $label = opcode_label($opcode);
        if (!isset($stats[$label])) {
            $stats[$label] = 0;
        }
        $stats[$label]++;
    }
    ksort($stats);
    return $stats;
}

function build_literal_xrefs(array $opcodes)
{
    $xrefs = array();
    foreach ($opcodes as $index => $opcode) {
        if (!is_array($opcode)) {
            continue;
        }
        foreach (array('op1', 'op2', 'result') as $operand) {
            $key = $operand . '.constant';
            if (!isset($opcode[$key]) || !isset($opcode[$operand . '.literal'])) {
                continue;
            }
            $literalIndex = (int)$opcode[$key];
            if (!isset($xrefs[$literalIndex])) {
                $xrefs[$literalIndex] = array();
            }
            $xrefs[$literalIndex][] = array(
                'opcode_index' => $index,
                'opcode_name' => opcode_label($opcode),
                'operand' => $operand,
            );
        }
    }
    ksort($xrefs);
    return $xrefs;
}

function build_decompile_sites(array $opcodes)
{
    $sites = array(
        'calls' => array(),
        'include_eval' => array(),
        'lambda_declarations' => array(),
        'assignments' => array(),
        'returns' => array(),
    );

    foreach ($opcodes as $index => $opcode) {
        if (!is_array($opcode)) {
            continue;
        }
        $label = opcode_label($opcode);
        $entry = array(
            'opcode_index' => $index,
            'opcode_name' => $label,
            'line' => isset($opcode['lineno']) ? $opcode['lineno'] : null,
        );

        if (in_array($label, array('ZEND_INIT_FCALL', 'ZEND_INIT_FCALL_BY_NAME', 'ZEND_DO_FCALL', 'ZEND_DO_FCALL_BY_NAME'), true)) {
            $entry['op1'] = operand_ir($opcode, 'op1');
            $entry['op2'] = operand_ir($opcode, 'op2');
            $sites['calls'][] = $entry;
        } elseif ($label === 'ZEND_INCLUDE_OR_EVAL') {
            $entry['op1'] = operand_ir($opcode, 'op1');
            $entry['extended_value'] = isset($opcode['extended_value']) ? $opcode['extended_value'] : null;
            $sites['include_eval'][] = $entry;
        } elseif ($label === 'ZEND_DECLARE_LAMBDA_FUNCTION') {
            $entry['op1'] = operand_ir($opcode, 'op1');
            $entry['result'] = operand_ir($opcode, 'result');
            $sites['lambda_declarations'][] = $entry;
        } elseif (strpos($label, 'ZEND_ASSIGN') === 0) {
            $entry['op1'] = operand_ir($opcode, 'op1');
            $entry['op2'] = operand_ir($opcode, 'op2');
            $entry['result'] = operand_ir($opcode, 'result');
            $sites['assignments'][] = $entry;
        } elseif ($label === 'ZEND_RETURN') {
            $entry['op1'] = operand_ir($opcode, 'op1');
            $sites['returns'][] = $entry;
        }
    }

    return $sites;
}

// PHP 7.2 fn_flags bitmask — zend_compile.h
// ZEND_ACC_STATIC=0x01 ABSTRACT=0x02 FINAL=0x04 PUBLIC=0x100 PROTECTED=0x200 PRIVATE=0x400
// ZEND_ACC_VARIADIC=0x1000000 RETURN_REFERENCE=0x4000000 HAS_RETURN_TYPE=0x40000000
function decode_fn_flags($flags)
{
    $flags = (int)$flags;
    $vis = 'none';
    if ($flags & 0x100)      $vis = 'public';
    elseif ($flags & 0x200)  $vis = 'protected';
    elseif ($flags & 0x400)  $vis = 'private';

    return array(
        'visibility'        => $vis,
        'is_static'         => (bool)($flags & 0x01),
        'is_abstract'       => (bool)($flags & 0x02),
        'is_final'          => (bool)($flags & 0x04),
        'is_ctor'           => (bool)($flags & 0x2000),
        'is_dtor'           => (bool)($flags & 0x4000),
        'is_deprecated'     => (bool)($flags & 0x40000),
        'is_closure'        => (bool)($flags & 0x100000),
        'is_generator'      => (bool)($flags & 0x800000),
        'is_variadic'       => (bool)($flags & 0x1000000),
        'returns_reference' => (bool)($flags & 0x4000000),
        'has_return_type'   => (bool)($flags & 0x40000000),
    );
}

// PHP 7.2 ce_flags bitmask — zend_compile.h
// ZEND_ACC_FINAL=0x04 IMPLICIT_ABSTRACT=0x10 EXPLICIT_ABSTRACT=0x20
// ZEND_ACC_INTERFACE=0x40 ZEND_ACC_TRAIT=0x80 ZEND_ACC_ANON_CLASS=0x100
function decode_ce_flags($flags)
{
    $flags = (int)$flags;
    return array(
        'is_interface'         => (bool)($flags & 0x40),
        'is_trait'             => (bool)($flags & 0x80),
        'is_abstract'          => (bool)(($flags & 0x10) || ($flags & 0x20)),
        'is_explicit_abstract' => (bool)($flags & 0x20),
        'is_final'             => (bool)($flags & 0x04),
        'is_anon'              => (bool)($flags & 0x100),
    );
}

// PHP 7.2 property flags — same visibility bits as fn_flags, plus ZEND_ACC_STATIC=0x01
function decode_property_flags($flags)
{
    $flags = (int)$flags;
    $vis = 'public';
    if ($flags & 0x100)      $vis = 'public';
    elseif ($flags & 0x200)  $vis = 'protected';
    elseif ($flags & 0x400)  $vis = 'private';

    return array(
        'visibility' => $vis,
        'is_static'  => (bool)($flags & 0x01),
    );
}

function arg_info_ir($argInfoArray, $numArgs, $requiredNumArgs)
{
    if (!is_array($argInfoArray) || empty($argInfoArray)) {
        return array();
    }

    // PHP 7.2 type codes from zend_types.h
    $typeCodes = array(
        1 => 'null', 2 => 'false', 3 => 'bool', 4 => 'int',
        5 => 'float', 6 => 'string', 7 => 'array', 8 => 'object', 13 => 'bool',
    );

    $result = array();
    foreach ($argInfoArray as $index => $argInfo) {
        if (!is_array($argInfo)) {
            continue;
        }

        $name      = isset($argInfo['name']) && is_string($argInfo['name']) ? $argInfo['name'] : null;
        $className = isset($argInfo['class_name']) && is_string($argInfo['class_name']) && $argInfo['class_name'] !== ''
            ? $argInfo['class_name'] : null;
        $typeCode  = isset($argInfo['type']) ? (int)$argInfo['type'] : 0;
        $typeName  = $className !== null
            ? $className
            : (isset($typeCodes[$typeCode]) ? $typeCodes[$typeCode] : ($typeCode > 0 ? 'type(' . $typeCode . ')' : null));

        $hasDefault = $numArgs !== null && $requiredNumArgs !== null
            ? ((int)$index >= (int)$requiredNumArgs)
            : null;

        $result[] = array(
            'index'             => (int)$index,
            'name'              => $name,
            'type_name'         => $typeName,
            'class_name'        => $className,
            'type_code'         => $typeCode,
            'pass_by_reference' => isset($argInfo['pass_by_reference']) ? (bool)$argInfo['pass_by_reference'] : false,
            'allow_null'        => isset($argInfo['allow_null'])        ? (bool)$argInfo['allow_null']        : false,
            'is_variadic'       => isset($argInfo['is_variadic'])       ? (bool)$argInfo['is_variadic']       : false,
            'has_default'       => $hasDefault,
        );
    }
    return $result;
}

// Decode return type from arg_info[-1] (extracted by C extension as return_type_info)
function return_type_ir($rawReturnType)
{
    if (!is_array($rawReturnType)) {
        return null;
    }

    $typeCodes = array(
        1 => 'null', 2 => 'false', 3 => 'bool', 4 => 'int',
        5 => 'float', 6 => 'string', 7 => 'array', 8 => 'object', 13 => 'bool',
    );

    $className = isset($rawReturnType['class_name']) && is_string($rawReturnType['class_name']) && $rawReturnType['class_name'] !== ''
        ? $rawReturnType['class_name'] : null;
    $typeCode  = isset($rawReturnType['type']) ? (int)$rawReturnType['type'] : 0;
    $typeName  = $className !== null
        ? $className
        : (isset($typeCodes[$typeCode]) ? $typeCodes[$typeCode] : ($typeCode > 0 ? 'type(' . $typeCode . ')' : null));

    return array(
        'type_name'  => $typeName,
        'class_name' => $className,
        'type_code'  => $typeCode,
        'allow_null' => isset($rawReturnType['allow_null']) ? (bool)$rawReturnType['allow_null'] : false,
    );
}

function live_range_ir($liveRangeArray)
{
    $result = array();
    foreach ($liveRangeArray as $entry) {
        if (!is_array($entry)) {
            continue;
        }
        $result[] = array(
            'var'   => isset($entry['var'])   ? $entry['var']   : null,
            'start' => isset($entry['start']) ? $entry['start'] : null,
            'end'   => isset($entry['end'])   ? $entry['end']   : null,
        );
    }
    return $result;
}

function try_catch_ir(array $tryCatchArray)
{
    $result = array();
    foreach ($tryCatchArray as $entry) {
        if (!is_array($entry)) {
            continue;
        }
        $result[] = array(
            'try_op'     => isset($entry['try_op'])     ? $entry['try_op']     : null,
            'catch_op'   => isset($entry['catch_op'])   ? $entry['catch_op']   : null,
            'finally_op' => isset($entry['finally_op']) ? $entry['finally_op'] : null,
            'finally_end'=> isset($entry['finally_end'])? $entry['finally_end']: null,
        );
    }
    return $result;
}

function op_array_ir($id, $kind, array $opArray, array $meta = array())
{
    $literals = isset($opArray['literals']) && is_array($opArray['literals']) ? $opArray['literals'] : array();
    $opcodes  = isset($opArray['opcodes'])  && is_array($opArray['opcodes'])  ? $opArray['opcodes']  : array();
    $literalIr = array();
    $opcodeIr  = array();

    foreach ($literals as $index => $literal) {
        $literalIr[] = literal_ir($literal, $index);
    }
    foreach ($opcodes as $index => $opcode) {
        if (is_array($opcode)) {
            $opcodeIr[] = opcode_ir($opcode, $index);
        }
    }

    $tryCatch  = isset($opArray['try_catch_array']) && is_array($opArray['try_catch_array'])
        ? try_catch_ir($opArray['try_catch_array']) : array();
    $liveRange = isset($opArray['live_range']) && is_array($opArray['live_range'])
        ? live_range_ir($opArray['live_range']) : array();

    $fnFlags         = isset($opArray['fn_flags'])         ? (int)$opArray['fn_flags']         : null;
    $numArgs         = isset($opArray['num_args'])         ? (int)$opArray['num_args']         : null;
    $requiredNumArgs = isset($opArray['required_num_args'])? (int)$opArray['required_num_args'] : null;
    $rawArgInfo      = isset($opArray['arg_info']) && is_array($opArray['arg_info']) ? $opArray['arg_info'] : null;
    $rawReturnType   = isset($opArray['return_type_info']) && is_array($opArray['return_type_info']) ? $opArray['return_type_info'] : null;

    return array(
        'id'                 => $id,
        'kind'               => $kind,
        'meta'               => $meta,
        'function_name'      => isset($opArray['function_name']) ? safe_value($opArray['function_name']) : safe_value(null),
        'filename'           => isset($opArray['filename'])      ? safe_value($opArray['filename'])      : safe_value(null),
        'scope'              => isset($opArray['scope'])         ? safe_value($opArray['scope'])         : safe_value(null),
        'line_start'         => isset($opArray['line_start'])    ? $opArray['line_start']                : null,
        'line_end'           => isset($opArray['line_end'])      ? $opArray['line_end']                  : null,
        'fn_flags'           => $fnFlags,
        'fn_flags_decoded'   => $fnFlags !== null ? decode_fn_flags($fnFlags) : null,
        'num_args'           => $numArgs,
        'required_num_args'  => $requiredNumArgs,
        'arg_info'           => arg_info_ir($rawArgInfo, $numArgs, $requiredNumArgs),
        'return_type_info'   => return_type_ir($rawReturnType),
        'doc_comment'        => isset($opArray['doc_comment']) ? safe_value($opArray['doc_comment']) : safe_value(null),
        'vars'               => isset($opArray['vars'])        ? safe_value($opArray['vars'])        : safe_value(null),
        'last_var'           => isset($opArray['last_var'])    ? $opArray['last_var']                : null,
        'T'                  => isset($opArray['T'])           ? $opArray['T']                       : null,
        'static_variables'   => isset($opArray['static_variables']) && is_array($opArray['static_variables'])
            ? safe_value($opArray['static_variables']) : safe_value(null),
        'try_catch'          => $tryCatch,
        'live_range'         => $liveRange,
        'literals'           => $literalIr,
        'opcodes'            => $opcodeIr,
        'cfg'                => build_cfg($opcodes),
        'analysis'           => array(
            'opcode_stats'   => build_opcode_stats($opcodes),
            'literal_xrefs'  => build_literal_xrefs($opcodes),
            'sites'          => build_decompile_sites($opcodes),
        ),
        'closure_report'     => isset($opArray['closure_report']) ? $opArray['closure_report'] : array(),
    );
}

function add_ir_op_array(array &$ir, $id, $kind, array $opArray, array $meta = array())
{
    if (isset($ir['op_arrays'][$id])) {
        return;
    }

    $ir['op_arrays'][$id] = op_array_ir($id, $kind, $opArray, $meta);
}

function build_decompile_ir($sourcePath, array $dump)
{
    $ir = array(
        'format' => 'ic72dump-ir-v3',
        'source_file' => $sourcePath,
        'summary' => array(
            'op_array_count' => 0,
            'function_count' => 0,
            'closure_count' => 0,
            'class_count' => isset($dump['class_table']) && is_array($dump['class_table']) ? count($dump['class_table']) : 0,
        ),
        'entry' => 'main',
        'op_arrays' => array(),
        'function_index' => array(),
        'closure_index' => array(),
        'class_index' => array(),
    );

    if (isset($dump['op_array']) && is_array($dump['op_array'])) {
        add_ir_op_array($ir, 'main', 'main', $dump['op_array']);

        if (isset($dump['op_array']['closures']) && is_array($dump['op_array']['closures'])) {
            foreach ($dump['op_array']['closures'] as $closure) {
                if (!is_array($closure) || !isset($closure['op_array']) || !is_array($closure['op_array'])) {
                    continue;
                }
                $lambdaHex = isset($closure['lambda_id_hex']) ? $closure['lambda_id_hex'] : sha1(serialize($closure));
                $id = 'closure:' . $lambdaHex;
                add_ir_op_array($ir, $id, 'closure', $closure['op_array'], array(
                    'lambda_id' => isset($closure['lambda_id']) ? safe_value($closure['lambda_id']) : safe_value(null),
                    'lambda_id_hex' => $lambdaHex,
                    'literal_index' => isset($closure['literal_index']) ? $closure['literal_index'] : null,
                    'opcode_index' => isset($closure['opcode_index']) ? $closure['opcode_index'] : null,
                    'parent' => 'main',
                ));
                $ir['closure_index'][$lambdaHex] = $id;
            }
        }
    }

    if (isset($dump['function_table']) && is_array($dump['function_table'])) {
        foreach ($dump['function_table'] as $name => $entry) {
            if (!is_array($entry) || !isset($entry['op_array']) || !is_array($entry['op_array'])) {
                continue;
            }
            $keyHex = isset($entry['function_table_key_hex']) ? $entry['function_table_key_hex'] : bin2hex((string)$name);
            $isClosure = isset($entry['function_table_key']) && is_string($entry['function_table_key']) && strpos($entry['function_table_key'], '{closure}') !== false;
            $id = ($isClosure ? 'closure:' : 'function:') . $keyHex;
            add_ir_op_array($ir, $id, $isClosure ? 'closure' : 'function', $entry['op_array'], array(
                'function_table_key' => isset($entry['function_table_key']) ? safe_value($entry['function_table_key']) : safe_value($name),
                'function_table_key_hex' => $keyHex,
            ));
            if ($isClosure) {
                $ir['closure_index'][$keyHex] = $id;
            } else {
                $ir['function_index'][$keyHex] = $id;
            }
        }
    }

    if (isset($dump['class_table']) && is_array($dump['class_table'])) {
        foreach ($dump['class_table'] as $className => $classInfo) {
            $classId  = 'class:' . bin2hex((string)$className);
            $ceFlags  = isset($classInfo['ce_flags']) ? (int)$classInfo['ce_flags'] : null;

            // Build properties_info with flags_decoded + default_value (now from C extension directly)
            $propertiesInfo = array();
            if (isset($classInfo['properties_info']) && is_array($classInfo['properties_info'])) {
                foreach ($classInfo['properties_info'] as $propName => $propInfo) {
                    if (!is_array($propInfo)) {
                        $propertiesInfo[(string)$propName] = safe_value($propInfo);
                        continue;
                    }
                    $propFlags = isset($propInfo['flags']) ? (int)$propInfo['flags'] : null;
                    $propertiesInfo[(string)$propName] = array(
                        'name'              => isset($propInfo['name'])            ? safe_value($propInfo['name'])            : safe_value(null),
                        'flags'             => $propFlags,
                        'flags_decoded'     => $propFlags !== null ? decode_property_flags($propFlags) : null,
                        'offset'            => isset($propInfo['offset'])          ? $propInfo['offset']                      : null,
                        'doc_comment'       => isset($propInfo['doc_comment'])     ? safe_value($propInfo['doc_comment'])     : safe_value(null),
                        'class_name'        => isset($propInfo['class_name'])      ? safe_value($propInfo['class_name'])      : safe_value(null),
                        // default_value now comes directly from C (fixed bug where prop was computed but unused)
                        'has_default_value' => !empty($propInfo['has_default_value']),
                        'default_value'     => array_key_exists('default_value', $propInfo)
                            ? safe_value($propInfo['default_value']) : null,
                    );
                }
            }

            // Build properties_merged: join properties_info + default values from the right table.
            // Instance props are indexed positionally in default_properties_table sorted by offset.
            // Static props use offset as direct index into default_static_members_table.
            $propertiesMerged = array();
            $defaultInstTable  = isset($classInfo['default_properties_table'])     && is_array($classInfo['default_properties_table'])     ? $classInfo['default_properties_table']     : array();
            $defaultStatTable  = isset($classInfo['default_static_members_table']) && is_array($classInfo['default_static_members_table']) ? $classInfo['default_static_members_table'] : array();

            // Separate instance/static, sort instance by offset to get positional index
            $instanceProps = array();
            $staticProps   = array();
            foreach ($propertiesInfo as $propName => $propData) {
                $isStatic = isset($propData['flags_decoded']) && !empty($propData['flags_decoded']['is_static']);
                if ($isStatic) {
                    $staticProps[$propName] = $propData;
                } else {
                    $instanceProps[$propName] = $propData;
                }
            }
            uasort($instanceProps, function ($a, $b) {
                $oa = isset($a['offset']) ? (int)$a['offset'] : 0;
                $ob = isset($b['offset']) ? (int)$b['offset'] : 0;
                return $oa - $ob;
            });

            $instIdx = 0;
            foreach ($instanceProps as $propName => $propData) {
                $defaultVal = array_key_exists($instIdx, $defaultInstTable)
                    ? safe_value($defaultInstTable[$instIdx]) : null;
                $propertiesMerged[$propName] = array(
                    'name'          => $propName,
                    'flags'         => $propData['flags'],
                    'flags_decoded' => $propData['flags_decoded'],
                    'default_value' => $defaultVal,
                    'is_static'     => false,
                    'doc_comment'   => isset($propData['doc_comment']) ? $propData['doc_comment'] : null,
                );
                $instIdx++;
            }
            foreach ($staticProps as $propName => $propData) {
                $offset     = isset($propData['offset']) ? (int)$propData['offset'] : -1;
                $defaultVal = ($offset >= 0 && array_key_exists($offset, $defaultStatTable))
                    ? safe_value($defaultStatTable[$offset]) : null;
                $propertiesMerged[$propName] = array(
                    'name'          => $propName,
                    'flags'         => $propData['flags'],
                    'flags_decoded' => $propData['flags_decoded'],
                    'default_value' => $defaultVal,
                    'is_static'     => true,
                    'doc_comment'   => isset($propData['doc_comment']) ? $propData['doc_comment'] : null,
                );
            }

            // Build constants_table with safe_value per constant value
            $constantsTable = array();
            if (isset($classInfo['constants_table']) && is_array($classInfo['constants_table'])) {
                foreach ($classInfo['constants_table'] as $constName => $constInfo) {
                    if (!is_array($constInfo)) {
                        $constantsTable[(string)$constName] = safe_value($constInfo);
                        continue;
                    }
                    $constantsTable[(string)$constName] = array(
                        'value'       => array_key_exists('value', $constInfo) ? safe_value($constInfo['value']) : null,
                        'doc_comment' => isset($constInfo['doc_comment']) ? safe_value($constInfo['doc_comment']) : safe_value(null),
                        'ce'          => isset($constInfo['ce'])          ? safe_value($constInfo['ce'])          : safe_value(null),
                    );
                }
            }

            $ir['class_index'][$classId] = array(
                'name'                         => safe_value($className),
                'ce_flags'                     => $ceFlags,
                'ce_flags_decoded'             => $ceFlags !== null ? decode_ce_flags($ceFlags) : null,
                'parent'                       => isset($classInfo['parent'])     ? safe_value($classInfo['parent'])     : safe_value(null),
                'interfaces'                   => isset($classInfo['interfaces']) && is_array($classInfo['interfaces']) ? array_values($classInfo['interfaces']) : array(),
                'traits'                       => isset($classInfo['traits'])     && is_array($classInfo['traits'])     ? array_values($classInfo['traits'])     : array(),
                'properties_info'              => $propertiesInfo,
                'properties_merged'            => $propertiesMerged,
                'constants_table'              => $constantsTable,
                'default_properties_table'     => $defaultInstTable  ? safe_value($defaultInstTable)  : null,
                'default_static_members_table' => $defaultStatTable  ? safe_value($defaultStatTable)  : null,
                'methods'                      => array(),
            );

            if (isset($classInfo['function_table']) && is_array($classInfo['function_table'])) {
                foreach ($classInfo['function_table'] as $methodName => $entry) {
                    if (!is_array($entry) || !isset($entry['op_array']) || !is_array($entry['op_array'])) {
                        continue;
                    }
                    $methodId = $classId . ':method:' . bin2hex((string)$methodName);
                    add_ir_op_array($ir, $methodId, 'method', $entry['op_array'], array(
                        'class'       => $classId,
                        'method_name' => safe_value($methodName),
                    ));
                    $ir['class_index'][$classId]['methods'][(string)$methodName] = $methodId;
                }
            }
        }
    }

    $ir['summary']['op_array_count'] = count($ir['op_arrays']);
    $ir['summary']['function_count'] = count($ir['function_index']);
    $ir['summary']['closure_count'] = count($ir['closure_index']);

    return $ir;
}

function export_op_array(array $opArray)
{
    $maxExportedLiterals = 60;
    $maxExportedOpcodes = 120;
    $opcodes = isset($opArray['opcodes']) && is_array($opArray['opcodes']) ? $opArray['opcodes'] : array();
    $literals = isset($opArray['literals']) && is_array($opArray['literals']) ? $opArray['literals'] : array();
    $vars = isset($opArray['vars']) && is_array($opArray['vars']) ? $opArray['vars'] : array();
    $closureReport = isset($opArray['closure_report']) && is_array($opArray['closure_report']) ? $opArray['closure_report'] : array();

    $exportedOpcodes = array();
    foreach ($opcodes as $index => $opcode) {
        if ($index >= $maxExportedOpcodes) {
            break;
        }
        $exportedOpcodes[] = array(
            'index' => $index,
            'line' => isset($opcode['lineno']) ? $opcode['lineno'] : null,
            'opcode' => isset($opcode['opcode']) ? $opcode['opcode'] : null,
            'opcode_name' => opcode_label($opcode),
            'op1_type' => isset($opcode['op1_type']) ? $opcode['op1_type'] : null,
            'op2_type' => isset($opcode['op2_type']) ? $opcode['op2_type'] : null,
            'result_type' => isset($opcode['result_type']) ? $opcode['result_type'] : null,
            'extended_value' => isset($opcode['extended_value']) ? $opcode['extended_value'] : null,
        );
    }

    $exportedLiterals = array();
    foreach ($literals as $index => $literal) {
        if ($index >= $maxExportedLiterals) {
            break;
        }
        $exportedLiterals[] = array(
            'index' => $index,
            'value' => describe_value($literal),
        );
    }

    return array(
        'function_name' => isset($opArray['function_name']) ? $opArray['function_name'] : null,
        'line_start' => isset($opArray['line_start']) ? $opArray['line_start'] : null,
        'line_end' => isset($opArray['line_end']) ? $opArray['line_end'] : null,
        'opcode_count' => count($opcodes),
        'literal_count' => count($literals),
        'exported_opcode_count' => count($exportedOpcodes),
        'exported_literal_count' => count($exportedLiterals),
        'var_count' => count($vars),
        'vars' => array_values($vars),
        'closure_report' => $closureReport,
        'literals' => $exportedLiterals,
        'opcodes' => $exportedOpcodes,
    );
}

function build_export_dump($sourcePath, array $dump)
{
    $opArray = isset($dump['op_array']) && is_array($dump['op_array']) ? $dump['op_array'] : array();
    $export = array(
        'source_file' => $sourcePath,
        'main' => export_op_array($opArray),
        'functions' => array(),
        'classes' => array(),
    );

    return $export;
}

function print_summary($path, array $export)
{
    $main = isset($export['main']) && is_array($export['main']) ? $export['main'] : array();
    $opcodes = isset($main['opcodes']) && is_array($main['opcodes']) ? $main['opcodes'] : array();
    $vars = isset($main['vars']) && is_array($main['vars']) ? $main['vars'] : array();

    echo "== ", $path, " ==\n";
    echo "lines: ", isset($main['line_start']) ? $main['line_start'] : '?', "-", isset($main['line_end']) ? $main['line_end'] : '?', "\n";
    echo "opcodes: ", isset($main['opcode_count']) ? $main['opcode_count'] : count($opcodes), " | literals: ", isset($main['literal_count']) ? $main['literal_count'] : 0, " | vars: ", count($vars), "\n";
    echo "exported preview: ", count($opcodes), " opcodes | ", isset($main['exported_literal_count']) ? $main['exported_literal_count'] : 0, " literals\n";
    if (isset($main['closure_report']) && is_array($main['closure_report'])) {
        $declared = isset($main['closure_report']['declared_lambdas']) ? $main['closure_report']['declared_lambdas'] : 0;
        $dumped = isset($main['closure_report']['dumped_closure_op_arrays']) ? $main['closure_report']['dumped_closure_op_arrays'] : 0;
        $missing = isset($main['closure_report']['missing']) ? $main['closure_report']['missing'] : 0;
        echo "closures: declared=", $declared, " | dumped=", $dumped, " | missing=", $missing, "\n";
    }

    if ($vars) {
        echo "vars: ", implode(', ', array_slice($vars, 0, 12));
        if (count($vars) > 12) {
            echo ", ...";
        }
        echo "\n";
    }

    echo "first opcodes:\n";
    $limit = min(12, count($opcodes));
    for ($index = 0; $index < $limit; $index++) {
        $opcode = $opcodes[$index];
        $line = isset($opcode['line']) ? $opcode['line'] : '?';
        $numeric = isset($opcode['opcode']) ? $opcode['opcode'] : '?';
        $label = isset($opcode['opcode_name']) ? $opcode['opcode_name'] : '#?';
        echo sprintf("  [%03d] line %-4s %-24s (%s)\n", $index, $line, $label, $numeric);
    }

    echo "\n";
}

$scriptDir = __DIR__;
$targets = array_slice($argv, 1);

if (!$targets) {
    $targets = array(
        $scriptDir . DIRECTORY_SEPARATOR . 'demo.php',
    );
}

$exitCode = 0;

foreach ($targets as $target) {
    $resolved = resolve_target($target, $scriptDir);
    if ($resolved === false) {
        fwrite(STDERR, "Cannot find file: {$target}\n");
        $exitCode = 1;
        continue;
    }

    $rawDump = dasm_file($resolved);
    $normalizedDump = normalize_dump($rawDump);
    $dump = prepare_dump_for_output($normalizedDump, $resolved);
    $export = build_export_dump($resolved, $dump);
    $ir = build_decompile_ir($resolved, $dump);
    $outputPath = dump_output_path($resolved);
    $jsonOutputPath = dump_json_output_path($resolved);
    $serialized = print_r($dump, true);
    $json = json_encode($ir, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);

    if ($serialized === false || file_put_contents($outputPath, $serialized . PHP_EOL) === false) {
        fwrite(STDERR, "Failed to write dump file: {$outputPath}\n");
        $exitCode = 1;
        continue;
    }

    if ($json === false || file_put_contents($jsonOutputPath, $json . PHP_EOL) === false) {
        fwrite(STDERR, "Failed to write JSON dump file: {$jsonOutputPath}\n");
        $exitCode = 1;
        continue;
    }

    print_summary($resolved, $export);
    echo "saved: ", $outputPath, "\n";
    echo "saved: ", $jsonOutputPath, "\n\n";
}

exit($exitCode);
