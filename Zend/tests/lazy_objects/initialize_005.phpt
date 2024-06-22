--TEST--
Lazy objects: ReflectionClass::initialize error
--FILE--
<?php

class C {
    public int $a;
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    $reflector = new ReflectionClass($obj);
    var_dump($reflector?->isInitialized($obj));

    try {
        var_dump($reflector?->initialize($obj));
    } catch (Exception $e) {
        printf("%s\n", $e->getMessage());
    }

    var_dump($reflector?->isInitialized($obj));
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
(new ReflectionClass($obj))->resetAsLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    throw new \Exception('initializer exception');
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
(new ReflectionClass($obj))->resetAsLazyProxy($obj, function ($obj) {
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
