--TEST--
Lazy objects: Initializer effects are reverted after exception (dynamic properties, no default props, initialized hashtable)
--XFAIL--
Class without props is never lazy
--FILE--
<?php

#[AllowDynamicProperties]
class C {}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeInstanceLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 1;
    throw new Exception('initializer exception');
});

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
ReflectionLazyObjectFactory::makeInstanceLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 1;
    throw new Exception('initializer exception');
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeInstanceLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 1;
    throw new Exception('initializer exception');
});

// Initializer effects on the virtual proxy are not reverted
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
object(C)#%d (1) {
  ["a"]=>
  int(1)
}
Is lazy: 1
