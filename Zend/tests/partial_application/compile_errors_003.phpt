--TEST--
Closure application compile errors: named arguments must come after placeholder
--FILE--
<?php
foo(n: 5, ?);
?>
--EXPECTF--
Fatal error: Cannot use positional argument after named argument in %s on line %d
