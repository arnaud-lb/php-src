--TEST--
Lazy objects: isLazyObject() returns true if object is lazy and non initialized
--XFAIL--
isLazyObject method was removed
--FILE--
<?php

class C {
    public readonly int $a;

    public function __construct() {
        $this->a = 1;
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    var_dump(ReflectionLazyObjectFactory::isLazyObject($obj));
    var_dump($obj->a);
    var_dump(ReflectionLazyObjectFactory::isLazyObject($obj));
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

--EXPECT--
# Ghost:
bool(true)
string(11) "initializer"
int(1)
bool(false)
# Virtual:
bool(true)
string(11) "initializer"
int(1)
bool(false)
