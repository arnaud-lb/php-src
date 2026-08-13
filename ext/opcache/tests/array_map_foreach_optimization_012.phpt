--TEST--
array_map(): foreach optimization - function argument is evaluated once
--EXTENSIONS--
opcache
--INI--
opcache.enable=1
opcache.enable_cli=1
opcache.opt_debug_level=0x20000
--FILE--
<?php

function plusn($x, $n) {
    return $x + $n;
}

function get_function() {
    var_dump(__FUNCTION__);
    return new class {
        function __invoke($value) {
            return $value + 1;
        }
    };
}

class C {
    function __construct() {
        var_dump(__METHOD__);
    }
    static function f($value) {
        return $value + 1;
    }
}

$array = range(1, 2);

var_dump(array_map(get_function()(...), $array));

var_dump(array_map((new C)::f(...), $array));

$f = function ($value) use (&$f) {
    $f = 'dechex';
    return $value + 1;
};

var_dump(array_map($f(...), $array));

?>
--EXPECTF--
$_main:
     ; (lines=%d, args=%d, vars=%d, tmps=%d)
     ; (after optimizer)
     ; %s
0000 INIT_FCALL 2 %d string("range")
0001 SEND_VAL int(1) 1
0002 SEND_VAL int(2) 2
0003 T2 = DO_ICALL
0004 ASSIGN CV0($array) T2
0005 INIT_FCALL 1 %d string("var_dump")
0006 INIT_FCALL 0 %d string("get_function")
0007 T3 = DO_UCALL
0008 TYPE_ASSERT 131079 string("array_map") CV0($array)
0009 T2 = INIT_ARRAY 0 (packed) NEXT
0010 V6 = FE_RESET_R CV0($array) 0018
0011 T8 = FE_FETCH_R V6 T7 0018
0012 T9 = COPY_TMP T3
0013 INIT_DYNAMIC_CALL 1 T9
0014 SEND_VAL_EX T7 1
0015 T7 = DO_FCALL
0016 T2 = ADD_ARRAY_ELEMENT T7 T8
0017 JMP 0011
0018 FE_FREE V6
0019 FREE T3
0020 SEND_VAL T2 1
0021 DO_ICALL
0022 INIT_FCALL 1 %d string("var_dump")
0023 T2 = NEW 0 string("C")
0024 DO_FCALL
0025 V7 = FETCH_CLASS (exception) T2
0026 TYPE_ASSERT 131079 string("array_map") CV0($array)
0027 T2 = INIT_ARRAY 0 (packed) NEXT
0028 V3 = FE_RESET_R CV0($array) 0035
0029 T6 = FE_FETCH_R V3 T4 0035
0030 INIT_STATIC_METHOD_CALL 1 V7 string("f")
0031 SEND_VAL_EX T4 1
0032 T4 = DO_FCALL
0033 T2 = ADD_ARRAY_ELEMENT T4 T6
0034 JMP 0029
0035 FE_FREE V3
0036 SEND_VAL T2 1
0037 DO_ICALL
0038 T2 = DECLARE_LAMBDA_FUNCTION 4294967295 0
0039 BIND_LEXICAL (ref) T2 CV1($f)
0040 ASSIGN CV1($f) T2
0041 INIT_FCALL 1 %d string("var_dump")
0042 T3 = QM_ASSIGN CV1($f)
0043 TYPE_ASSERT 131079 string("array_map") CV0($array)
0044 T2 = INIT_ARRAY 0 (packed) NEXT
0045 V4 = FE_RESET_R CV0($array) 0053
0046 T6 = FE_FETCH_R V4 T5 0053
0047 T7 = COPY_TMP T3
0048 INIT_DYNAMIC_CALL 1 T7
0049 SEND_VAL_EX T5 1
0050 T5 = DO_FCALL
0051 T2 = ADD_ARRAY_ELEMENT T5 T6
0052 JMP 0046
0053 FE_FREE V4
0054 FREE T3
0055 SEND_VAL T2 1
0056 DO_ICALL
0057 RETURN int(1)
LIVE RANGES:
     3: 0008 - 0019 (tmp/var)
     2: 0010 - 0020 (tmp/var)
     6: 0011 - 0018 (loop)
     7: 0012 - 0014 (tmp/var)
     8: 0012 - 0016 (tmp/var)
     2: 0024 - 0025 (new)
     2: 0028 - 0036 (tmp/var)
     3: 0029 - 0035 (loop)
     4: 0030 - 0031 (tmp/var)
     6: 0030 - 0033 (tmp/var)
     2: 0039 - 0040 (tmp/var)
     3: 0043 - 0054 (tmp/var)
     2: 0045 - 0055 (tmp/var)
     4: 0046 - 0053 (loop)
     5: 0047 - 0049 (tmp/var)
     6: 0047 - 0051 (tmp/var)

{closure:%s:%d}:
     ; (lines=%d, args=%d, vars=%d, tmps=%d)
     ; (after optimizer)
     ; %s
0000 CV0($value) = RECV 1
0001 BIND_STATIC (ref) CV1($f)
0002 ASSIGN CV1($f) string("dechex")
0003 T2 = ADD CV0($value) int(1)
0004 RETURN T2

plusn:
     ; (lines=%d, args=%d, vars=%d, tmps=%d)
     ; (after optimizer)
     ; %s
0000 CV0($x) = RECV 1
0001 CV1($n) = RECV 2
0002 T2 = ADD CV0($x) CV1($n)
0003 RETURN T2

get_function:
     ; (lines=%d, args=%d, vars=%d, tmps=%d)
     ; (after optimizer)
     ; %s
0000 INIT_FCALL 1 %d string("var_dump")
0001 SEND_VAL string("get_function") 1
0002 DO_ICALL
0003 V1 = DECLARE_ANON_CLASS string("class@anonymous\x00/home/arnaud/dev/php/php-src/ext/opcache/tests/array_map_foreach_optimization_012.php:9$0")
0004 T0 = NEW 0 V1
0005 DO_FCALL
0006 RETURN T0
LIVE RANGES:
     0: 0005 - 0006 (new)

class@anonymous::__invoke:
     ; (lines=%d, args=%d, vars=%d, tmps=%d)
     ; (after optimizer)
     ; %s
0000 CV0($value) = RECV 1
0001 T1 = ADD CV0($value) int(1)
0002 RETURN T1

C::__construct:
     ; (lines=%d, args=%d, vars=%d, tmps=%d)
     ; (after optimizer)
     ; %s
0000 INIT_FCALL 1 %d string("var_dump")
0001 SEND_VAL string("C::__construct") 1
0002 DO_ICALL
0003 RETURN null

C::f:
     ; (lines=%d, args=%d, vars=%d, tmps=%d)
     ; (after optimizer)
     ; %s
0000 CV0($value) = RECV 1
0001 T1 = ADD CV0($value) int(1)
0002 RETURN T1
string(12) "get_function"
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(3)
}
string(14) "C::__construct"
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(3)
}
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(3)
}