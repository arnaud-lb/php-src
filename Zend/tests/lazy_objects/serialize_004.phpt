--TEST--
Lazy objects: serialize does not initializes object if flag is set (properties hashtable)
--FILE--
<?php

class C {
    public int $a;
    public int $b;
}


function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    ReflectionLazyObject::fromInstance($obj)->setProperty('b', 1);

    // Builds properties hashtable
    get_object_vars($obj);

    $serialized = serialize($obj);
    $unserialized = unserialize($serialized);
    var_dump($serialized, $unserialized);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function ($obj) {
    var_dump("initializer");
}, ReflectionLazyObject::SKIP_INITIALIZATION_ON_SERIALIZE);

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function ($obj) {
    var_dump("initializer");
}, ReflectionLazyObject::STRATEGY_VIRTUAL | ReflectionLazyObject::SKIP_INITIALIZATION_ON_SERIALIZE);

test('Virtual', $obj);

--EXPECTF--
# Ghost:
string(24) "O:1:"C":1:{s:1:"b";i:1;}"
object(C)#%d (1) {
  ["a"]=>
  uninitialized(int)
  ["b"]=>
  int(1)
}
# Virtual:
string(24) "O:1:"C":1:{s:1:"b";i:1;}"
object(C)#%d (1) {
  ["a"]=>
  uninitialized(int)
  ["b"]=>
  int(1)
}
