--TEST--
Lazy objects: exception during initializer leaves object uninitialized
--FILE--
<?php

class C {
    public int $a;
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    try {
        var_dump($obj->a);
    } catch (Exception $e) {
        printf("%s\n", $e->getMessage());
    }
    var_dump(!ReflectionLazyObjectFactory::isInitialized($obj));
    try {
        var_dump($obj->a);
    } catch (Exception $e) {
        printf("%s\n", $e->getMessage());
    }
    var_dump(!ReflectionLazyObjectFactory::isInitialized($obj));
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeInstanceLazyGhost($obj, function () {
    var_dump("initializer");
    throw new \Exception('initializer exception');
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeInstanceLazyProxy($obj, function () {
    var_dump("initializer");
    throw new \Exception('initializer exception');
});

test('Virtual', $obj);

--EXPECT--
# Ghost:
string(11) "initializer"
initializer exception
bool(true)
string(11) "initializer"
initializer exception
bool(true)
# Virtual:
string(11) "initializer"
initializer exception
bool(true)
string(11) "initializer"
initializer exception
bool(true)
