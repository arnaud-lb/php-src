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

    printf("Initialized:\n");
    var_dump($reflector?->isInitialized());

    var_dump($reflector?->initialize());

    printf("Initialized:\n");
    var_dump($reflector?->isInitialized());
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 1;
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyVirtual($obj, function ($obj) {
    var_dump("initializer");
    $c = new C();
    $c->a = 1;
    return $c;
});

test('Virtual', $obj);

--EXPECTF--
# Ghost:
Initialized:
bool(false)
string(11) "initializer"
object(C)#%d (1) {
  ["a"]=>
  int(1)
}
Initialized:
bool(true)
# Virtual:
Initialized:
bool(false)
string(11) "initializer"
object(C)#%d (1) {
  ["a"]=>
  int(1)
}
Initialized:
bool(true)
