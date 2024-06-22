--TEST--
Lazy objects: clone preserves initialized properties
--FILE--
<?php

class C {
    public $a = 1;
    public $b;

    public function __construct() {
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    (new ReflectionProperty($obj, 'a'))->setRawValueWithoutLazyInitialization($obj, 2);

    $clone = clone $obj;

    var_dump(!(new ReflectionClass($obj))->isInitialized($obj));
    var_dump($obj);
    var_dump(!(new ReflectionClass($clone))->isInitialized($clone));
    var_dump($clone);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
(new ReflectionClass($obj))->resetAsLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct();
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
(new ReflectionClass($obj))->resetAsLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    return new C();
});

test('Virtual', $obj);

--EXPECTF--
# Ghost:
bool(true)
object(C)#%d (1) {
  ["a"]=>
  int(2)
}
bool(true)
object(C)#%d (1) {
  ["a"]=>
  int(2)
}
# Virtual:
bool(true)
object(C)#%d (1) {
  ["a"]=>
  int(2)
}
bool(true)
object(C)#%d (1) {
  ["a"]=>
  int(2)
}
