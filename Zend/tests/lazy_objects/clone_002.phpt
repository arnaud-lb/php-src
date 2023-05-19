--TEST--
Lazy objects: clone preserves initialized properties
--FILE--
<?php

class C {
    public $a = 1;

    public function __construct() {
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    ReflectionLazyObject::fromInstance($obj)->setProperty('a', 2);

    $clone = clone $obj;

    var_dump((bool) ReflectionLazyObject::fromInstance($obj));
    var_dump($obj);
    var_dump((bool) ReflectionLazyObject::fromInstance($clone));
    var_dump($clone);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct();
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function ($obj) {
    var_dump("initializer");
    return new C();
}, ReflectionLazyObject::STRATEGY_VIRTUAL);

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
