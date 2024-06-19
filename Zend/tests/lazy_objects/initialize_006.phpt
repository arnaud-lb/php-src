--TEST--
Lazy objects: (new ReflectionClass(skipInitializer: true))->initialize(skipInitializer: true) initializes properties to their default value and skips initializer
--FILE--
<?php

class C {
    public int $a = 1;
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    $reflector = new ReflectionClass($obj);

    printf("Initialized:\n");
    var_dump($reflector?->isInitialized($obj));

    printf("initialize(true) returns \$obj:\n");
    var_dump($reflector?->initialize($obj, true) === $obj);

    printf("Initialized:\n");
    var_dump($reflector?->isInitialized($obj));
    var_dump($obj);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
(new ReflectionClass($obj))->resetAsLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 1;
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
(new ReflectionClass($obj))->resetAsLazyProxy($obj, function ($obj) {
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
