--TEST--
Closure application pipe optimization: PFA with both a variadic placeholder and named arg can not be optimized
--EXTENSIONS--
opcache
--INI--
opcache.opt_debug_level=0x20000
--FILE--
<?php

if (time() > 0) {
    function foo($a, $b) {
        var_dump($a, $b);
    }
}

try {
    2 |> foo(..., a: 1);
} catch (\Throwable $e) {
    echo $e->getMessage(), "\n";
}

?>
--EXPECTF--
$_main:
     ; (lines=20, args=0, vars=1, tmps=2)
     ; (after optimizer)
     ; %spipe_optimization_008.php:1-16
0000 INIT_FCALL 0 %d string("time")
0001 V2 = DO_ICALL
0002 T1 = IS_SMALLER int(0) V2
0003 JMPZ T1 0005
0004 DECLARE_FUNCTION string("foo") 0
0005 INIT_FCALL_BY_NAME 1 string("foo")
0006 SEND_PLACEHOLDER
0007 SEND_VAL_EX int(1) string("a")
0008 CHECK_PARTIAL_ARGS
0009 T1 = CALLABLE_CONVERT_PARTIAL string("{closure:%s}")
0010 INIT_DYNAMIC_CALL 1 T1
0011 SEND_VAL_EX int(2) 1
0012 DO_FCALL
0013 RETURN int(1)
0014 CV0($e) = CATCH string("Throwable")
0015 INIT_METHOD_CALL 0 CV0($e) string("getMessage")
0016 V1 = DO_FCALL
0017 ECHO V1
0018 ECHO string("\n")
0019 RETURN int(1)
EXCEPTION TABLE:
     0005, 0014, -, -

foo:
     ; (lines=7, args=2, vars=2, tmps=0)
     ; (after optimizer)
     ; %spipe_optimization_008.php:4-6
0000 CV0($a) = RECV 1
0001 CV1($b) = RECV 2
0002 INIT_FCALL 2 %d string("var_dump")
0003 SEND_VAR CV0($a) 1
0004 SEND_VAR CV1($b) 2
0005 DO_ICALL
0006 RETURN null
int(1)
int(2)
