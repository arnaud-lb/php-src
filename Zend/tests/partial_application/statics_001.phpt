--TEST--
Partial application static variables shared
--FILE--
<?php
function foo($a) {
    static $var = 0;

    ++$var;

    return $var;
}

foo(new stdClass);

$foo = foo(new stdClass, ...);

if ($foo() == 2) {
    echo "OK";
}
?>
--EXPECTF--
OK
