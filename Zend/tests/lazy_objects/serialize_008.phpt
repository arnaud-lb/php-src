--TEST--
Lazy objects: serialize does not initializes object with __sleep method if flag is set
--FILE--
<?php

class C {
    public int $a;
    public function __sleep() {
        return ['a'];
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    $serialized = serialize($obj);
    $unserialized = unserialize($serialized);
    var_dump($serialized, $unserialized);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 1;
}, ReflectionLazyObject::SKIP_INITIALIZATION_ON_SERIALIZE);

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 1;
}, ReflectionLazyObject::SKIP_INITIALIZATION_ON_SERIALIZE);

test('Virtual', $obj);

--EXPECTF--
# Ghost:
string(12) "O:1:"C":0:{}"
object(C)#%d (0) {
  ["a"]=>
  uninitialized(int)
}
# Virtual:
string(12) "O:1:"C":0:{}"
object(C)#%d (0) {
  ["a"]=>
  uninitialized(int)
}
