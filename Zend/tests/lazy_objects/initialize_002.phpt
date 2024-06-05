--TEST--
Lazy objects: ReflectionLazyObjectFactory::initialize on an initialized object is a no-op
--FILE--
<?php

class C {
    public int $a;
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    $reflector = ReflectionLazyObjectFactory::fromInstance($obj);

    var_dump($obj->a);

    var_dump($reflector?->isInitialized());

    var_dump($reflector?->initialize());

    var_dump($reflector?->isInitialized());
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 1;
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    $c = new C();
    $c->a = 1;
    return $c;
});

test('Virtual', $obj);

--EXPECT--
# Ghost:
string(11) "initializer"
int(1)
bool(true)
object(C)#2 (1) {
  ["a"]=>
  int(1)
}
bool(true)
# Virtual:
string(11) "initializer"
int(1)
bool(true)
object(C)#4 (1) {
  ["a"]=>
  int(1)
}
bool(true)
