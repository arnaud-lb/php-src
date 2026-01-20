--TEST--
PFA with $this placeholder: $this placeholder not allowed after variadic placeholder
--FILE--
<?php

class C {
    function f($a = null, $b = null) {
    }
}

C::f(..., $this: ?);

?>
--EXPECTF--
Fatal error: Variadic placeholder must be last in %s on line %d
