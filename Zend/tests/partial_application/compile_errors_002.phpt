--TEST--
Closure application compile errors: ... must be last
--FILE--
<?php
foo(..., ?);
?>
--EXPECTF--
Fatal error: Variadic placeholder must be last in %s on line %d
