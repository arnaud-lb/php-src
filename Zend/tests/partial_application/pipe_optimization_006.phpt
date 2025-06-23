--TEST--
Partial application pipe optimization: PFA with only one placeholder can be optimized (named)
--EXTENSIONS--
opcache
--INI--
opcache.opt_debug_level=0x20000
--FILE--
<?php

if (time() > 0) {
    function foo($a, $b = null, $c = null) {
        var_dump($a, $b, $c);
    }
}

2 |> foo(1, c: ?);

?>
--EXPECTF--
$_main:
     ; (lines=11, args=0, vars=0, tmps=2)
     ; (after optimizer)
     ; %spipe_optimization_006.php:1-12
0000 INIT_FCALL 0 80 string("time")
0001 V1 = DO_ICALL
0002 T0 = IS_SMALLER int(0) V1
0003 JMPZ T0 0005
0004 DECLARE_FUNCTION string("foo") 0
0005 INIT_FCALL_BY_NAME 1 string("foo")
0006 SEND_VAL_EX int(1) 1
0007 SEND_VAL_EX int(2) string("c")
0008 CHECK_UNDEF_ARGS
0009 DO_FCALL_BY_NAME
0010 RETURN int(1)

foo:
     ; (lines=9, args=3, vars=3, tmps=0)
     ; (after optimizer)
     ; %spipe_optimization_006.php:4-6
0000 CV0($a) = RECV 1
0001 CV1($b) = RECV_INIT 2 null
0002 CV2($c) = RECV_INIT 3 null
0003 INIT_FCALL 3 128 string("var_dump")
0004 SEND_VAR CV0($a) 1
0005 SEND_VAR CV1($b) 2
0006 SEND_VAR CV2($c) 3
0007 DO_ICALL
0008 RETURN null
int(1)
NULL
int(2)
