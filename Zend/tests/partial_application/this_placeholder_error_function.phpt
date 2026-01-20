--TEST--
PFA with $this placeholder: $this placeholder on free-standing function
--FILE--
<?php

$f = strlen($this: ?, ?);

?>
--EXPECTF--
Fatal error: Invalid use of $this placeholder in %s on line %d
