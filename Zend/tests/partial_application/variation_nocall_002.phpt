--TEST--
Closure application variation no call order of destruction
--FILE--
<?php
class Foo {
    function method($a, $b) {}
}
class Dtor {
    public function __construct(public int $id) {}
    public function __destruct() {
        echo __METHOD__, " ", $this->id, "\n";
    }
}
$foo = new Foo;
$foo->method(new Dtor(1), ...)(new Dtor(2), ...);

echo "OK";
?>
--EXPECT--
Dtor::__destruct 2
Dtor::__destruct 1
OK
