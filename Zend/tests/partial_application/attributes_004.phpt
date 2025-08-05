--TEST--
Closure application preserves #[Deprecated]
--FILE--
<?php

#[Deprecated] function f($a) {
}

$f = f(?);
$f(1);

?>
--EXPECTF--
Deprecated: Function {closure:%s}() is deprecated in %s on line 7
