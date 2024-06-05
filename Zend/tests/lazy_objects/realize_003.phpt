--TEST--
Lazy objects: Object with no props is never lazy
--FILE--
<?php

class C {}

#[AllowDynamicProperties]
class D {}

function test(string $name, object $obj, object $obj2) {
    printf("# %s:\n", $name);

    var_dump(!ReflectionLazyObject::fromInstance($obj)?->isInitialized());
    var_dump($obj);

    var_dump(!ReflectionLazyObject::fromInstance($obj2)?->isInitialized());
    var_dump($obj2);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
});

$obj2 = new D();
$obj2->dynamic = 'value';
ReflectionLazyObject::makeLazyGhost($obj2, function ($obj2) {
    var_dump("initializer");
});

test('Ghost', $obj, $obj2);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyProxy($obj, function ($obj) {
    var_dump("initializer");
});

$obj2 = new D();
$obj2->dynamic = 'value';
ReflectionLazyObject::makeLazyProxy($obj2, function ($obj2) {
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
