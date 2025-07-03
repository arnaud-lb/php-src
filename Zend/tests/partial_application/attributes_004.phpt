--TEST--
Partial application preserves #[Deprecated]
--FILE--
<?php

#[Deprecated] function f($a) {
}

$f = f(?);
$f(1);

?>
--EXPECTF--
Deprecated: Function f() is deprecated in %s on line 7
