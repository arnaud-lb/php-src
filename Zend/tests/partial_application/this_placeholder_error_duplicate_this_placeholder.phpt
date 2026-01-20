--TEST--
PFA with $this placeholder: $this placeholder may only appear once
--FILE--
<?php

class C {
    function f($a = null, $b = null) {
    }
}

C::f($this: ?, $this: ?);

?>
--EXPECTF--
Fatal error: $this placeholder may only appear once in %s on line %d
