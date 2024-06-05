--TEST--
Lazy objects: clone may not initialize object
--FILE--
<?php

class C {
    public $a = 1;

    public function __construct() {
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    $clone = clone $obj;

    var_dump(!ReflectionLazyObjectFactory::fromInstance($obj)?->isInitialized());
    var_dump($obj);
    var_dump(!ReflectionLazyObjectFactory::fromInstance($clone)?->isInitialized());
    var_dump($clone);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct();
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    return new C();
});

test('Virtual', $obj);

--EXPECTF--
# Ghost:
bool(true)
object(C)#%d (0) {
}
bool(true)
object(C)#%d (0) {
}
# Virtual:
bool(true)
object(C)#%d (0) {
}
bool(true)
object(C)#%d (0) {
}
