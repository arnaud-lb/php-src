--TEST--
Closure application compile errors: named arguments must come before variadic placeholder
--FILE--
<?php
foo(..., n: 5);
?>
--EXPECTF--
Fatal error: Variadic placeholder must be last in %s on line %d
