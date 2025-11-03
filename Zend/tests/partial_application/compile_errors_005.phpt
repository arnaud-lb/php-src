--TEST--
Closure application compile errors: follow variadic with un-named arg
--FILE--
<?php
foo(..., $a);
?>
--EXPECTF--
Fatal error: Variadic placeholder must be last in %s on line %d
