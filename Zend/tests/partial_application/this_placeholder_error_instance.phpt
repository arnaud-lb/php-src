--TEST--
PFA with $this placeholder: $this placeholder on instance method
--FILE--
<?php

$f = $foo->bar(this: ?);

?>
--EXPECTF--
Fatal error: Invalid use of 'this' placeholder in %s on line %d
