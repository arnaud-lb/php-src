--TEST--
Lazy objects: Initializer effects are reverted after exception (properties hashtable, no props)
--XFAIL--
Class without props is never lazy
--FILE--
<?php

class C {}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    // Builds properties hashtable
    var_dump(get_object_vars($obj));

    try {
        ReflectionLazyObjectFactory::initialize($obj);
    } catch (Exception $e) {
        printf("%s\n", $e->getMessage());
    }

    var_dump($obj);
    printf("Is lazy: %d\n", !ReflectionLazyObjectFactory::isInitialized($obj));
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    throw new Exception('initializer exception');
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    throw new Exception('initializer exception');
});

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
