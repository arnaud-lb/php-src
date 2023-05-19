--TEST--
Lazy objects: ReflectionLazyObject::initialize
--FILE--
<?php

class C {
    public int $a;
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    $reflector = ReflectionLazyObject::fromInstance($obj);
    var_dump($reflector?->isInitialized());

    var_dump($reflector?->initialize());

    var_dump($reflector?->isInitialized());
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 1;
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function ($obj) {
    var_dump("initializer");
    $c = new C();
    $c->a = 1;
    return $c;
}, ReflectionLazyObject::STRATEGY_VIRTUAL);

test('Virtual', $obj);

--EXPECT--
# Ghost:
bool(false)
string(11) "initializer"
object(C)#2 (1) {
  ["a"]=>
  int(1)
}
bool(true)
# Virtual:
bool(false)
string(11) "initializer"
object(C)#4 (1) {
  ["a"]=>
  int(1)
}
bool(true)
