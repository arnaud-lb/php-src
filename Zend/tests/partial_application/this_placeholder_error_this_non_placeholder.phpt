--TEST--
PFA with $this placeholder: $this name allowed only for placeholders
--FILE--
<?php

class C {
    function f($a = null, $b = null) {
    }
}

C::f($this: 1);

?>
--EXPECTF--
Parse error: syntax error, unexpected integer "1", expecting "?" in %s on line %d
