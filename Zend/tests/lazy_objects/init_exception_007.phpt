--TEST--
Lazy objects: Initializer effects are reverted after exception (dynamic properties, no default props)
--XFAIL--
Class without props is never lazy
--FILE--
<?php

#[AllowDynamicProperties]
class C {}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    try {
        ReflectionLazyObject::fromInstance($obj)->initialize();
    } catch (Exception $e) {
        printf("%s\n", $e->getMessage());
    }

    var_dump($obj);
    printf("Is lazy: %d\n", !ReflectionLazyObject::fromInstance($obj)?->isInitialized());
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 1;
    throw new Exception('initializer exception');
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 1;
    throw new Exception('initializer exception');
});

// Initializer effects on the virtual proxy are not reverted
test('Virtual', $obj);


--EXPECTF--
# Ghost:
string(11) "initializer"
initializer exception
object(C)#%d (0) {
}
Is lazy: 1
# Virtual:
string(11) "initializer"
initializer exception
object(C)#%d (1) {
  ["a"]=>
  int(1)
}
Is lazy: 1
