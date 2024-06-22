--TEST--
Lazy objects: Object with no props is never lazy
--FILE--
<?php

class C {}

#[AllowDynamicProperties]
class D {}

function test(string $name, object $obj, object $obj2) {
    printf("# %s:\n", $name);

    var_dump(!(new ReflectionClass($obj))->isInitialized($obj));
    var_dump($obj);

    var_dump(!(new ReflectionClass($obj2))->isInitialized($obj2));
    var_dump($obj2);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
(new ReflectionClass($obj))->resetAsLazyGhost($obj, function ($obj) {
    var_dump("initializer");
});

$obj2 = new D();
$obj2->dynamic = 'value';
(new ReflectionClass($obj2))->resetAsLazyGhost($obj2, function ($obj2) {
    var_dump("initializer");
});

test('Ghost', $obj, $obj2);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
(new ReflectionClass($obj))->resetAsLazyProxy($obj, function ($obj) {
    var_dump("initializer");
});

$obj2 = new D();
$obj2->dynamic = 'value';
(new ReflectionClass($obj2))->resetAsLazyProxy($obj2, function ($obj2) {
    var_dump("initializer");
});

test('Virtual', $obj, $obj2);

--EXPECTF--
# Ghost:
bool(false)
object(C)#%d (0) {
}
bool(false)
object(D)#%d (0) {
}
# Virtual:
bool(false)
object(C)#%d (0) {
}
bool(false)
object(D)#%d (0) {
}
