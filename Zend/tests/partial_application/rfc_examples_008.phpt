--TEST--
Closure application RFC examples: magic methods
--FILE--
<?php

class Foo {
    public function __call($method, $args) {
        printf("%s::%s\n", __CLASS__, $method);
        print_r($args);
    }
}

$f = new Foo();
$m = $f->method(?, ?);

$m(1, 2);

?>
--EXPECTF--
Foo::method
Array
(
    [0] => 1
    [1] => 2
)
