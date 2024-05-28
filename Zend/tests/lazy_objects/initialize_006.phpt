--TEST--
Lazy objects: ReflectionLazyObject::initialize(skipInitializer: true) initializes properties to their default value and skips initializer
--FILE--
<?php

class C {
    public int $a = 1;
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    $reflector = ReflectionLazyObject::fromInstance($obj);

    printf("Initialized:\n");
    var_dump($reflector?->isInitialized());

    printf("initialize(true) returns \$obj:\n");
    var_dump($reflector?->initialize(true) === $obj);

    printf("Initialized:\n");
    var_dump($reflector?->isInitialized());
    var_dump($obj);
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

--EXPECTF--
# Ghost:
Initialized:
bool(false)
initialize(true) returns $obj:
bool(true)
Initialized:
bool(true)
object(C)#%d (1) {
  ["a"]=>
  int(1)
}
# Virtual:
Initialized:
bool(false)
initialize(true) returns $obj:
bool(true)
Initialized:
bool(true)
object(C)#%d (1) {
  ["a"]=>
  int(1)
}
