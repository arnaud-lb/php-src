--TEST--
PFA with $this placeholder: $this placeholder can not be variadic
--FILE--
<?php

class C {
    function f($a = null, $b = null) {
    }
}

C::f(this: ...);

?>
--EXPECTF--
Parse error: syntax error, unexpected token "..." in %s on line %d
