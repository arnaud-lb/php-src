--TEST--
Lazy objects: ReflectionLazyObject::initialize error
--FILE--
<?php

class C {
    public int $a;
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    $reflector = ReflectionLazyObject::fromInstance($obj);
    var_dump($reflector?->isInitialized());

    try {
        var_dump($reflector?->initialize());
    } catch (Exception $e) {
        printf("%s\n", $e->getMessage());
    }

    var_dump($reflector?->isInitialized());
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    throw new \Exception('initializer exception');
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    throw new \Exception('initializer exception');
});

test('Virtual', $obj);

--EXPECT--
# Ghost:
bool(false)
string(11) "initializer"
initializer exception
bool(false)
# Virtual:
bool(false)
string(11) "initializer"
initializer exception
bool(false)
