--TEST--
Lazy objects: exception during initializer leaves object uninitialized
--FILE--
<?php

class C {
    public int $a;

    public function __destruct() {
        var_dump(__METHOD__);
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    try {
        var_dump($obj->a);
    } catch (Exception $e) {
        printf("%s\n", $e->getMessage());
    }
    var_dump(!ReflectionLazyObjectFactory::fromInstance($obj)?->isInitialized());
    try {
        var_dump($obj->a);
    } catch (Exception $e) {
        printf("%s\n", $e->getMessage());
    }
    var_dump(!ReflectionLazyObjectFactory::fromInstance($obj)?->isInitialized());
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeLazyGhost($obj, function () {
    var_dump("initializer");
    throw new \Exception('initializer exception');
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeLazyProxy($obj, function () {
    var_dump("initializer");
    throw new \Exception('initializer exception');
});

test('Virtual', $obj);
--EXPECTF--
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
