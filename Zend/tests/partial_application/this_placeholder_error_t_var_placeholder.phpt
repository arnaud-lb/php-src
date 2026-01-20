--TEST--
PFA with $this placeholder: T_VARIABLE placeholder can only be $this
--FILE--
<?php

class C {
    function f($a = null, $b = null) {
    }
}

C::f($test: ?);

?>
--EXPECTF--
Fatal error: Invalid parameter name: $test in %s on line %d
