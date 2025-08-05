--TEST--
Closure application variation no call order of destruction
--FILE--
<?php
class Foo {
    function method($a, $b) {}
}
$foo = new Foo;
$foo->method(new stdClass, ...)(new stdClass, ...);

echo "OK";
?>
--EXPECT--
OK
