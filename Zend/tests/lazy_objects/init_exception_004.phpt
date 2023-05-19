--TEST--
Lazy objects: Initializer effects are reverted after exception (properties hashtable, no props)
--FILE--
<?php

class C {}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    // Builds properties hashtable
    var_dump(get_object_vars($obj));

    try {
        ReflectionLazyObject::fromInstance($obj)->initialize();
    } catch (Exception $e) {
        printf("%s\n", $e->getMessage());
    }

    var_dump($obj);
    printf("Is lazy: %d\n", (bool) ReflectionLazyObject::fromInstance($obj));
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function ($obj) {
    var_dump("initializer");
    throw new Exception('initializer exception');
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function ($obj) {
    var_dump("initializer");
    throw new Exception('initializer exception');
}, ReflectionLazyObject::STRATEGY_VIRTUAL);

test('Virtual', $obj);

--EXPECTF--
# Ghost:
array(0) {
}
string(11) "initializer"
initializer exception
object(C)#%d (0) {
}
Is lazy: 1
# Virtual:
array(0) {
}
string(11) "initializer"
initializer exception
object(C)#%d (0) {
}
Is lazy: 1
