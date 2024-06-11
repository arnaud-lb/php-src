--TEST--
Lazy objects: ReflectionLazyObjectFactory::initialize
--FILE--
<?php

class C {
    public int $a;
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    $reflector = new ReflectionLazyObjectFactory($obj);

    printf("Initialized:\n");
    var_dump($reflector?->isInitialized($obj));

    var_dump($reflector?->initialize($obj));

    printf("Initialized:\n");
    var_dump($reflector?->isInitialized($obj));
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeInstanceLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 1;
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeInstanceLazyProxy($obj, function ($obj) {
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
