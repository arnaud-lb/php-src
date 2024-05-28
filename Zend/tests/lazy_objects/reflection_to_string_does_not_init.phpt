--TEST--
Lazy objects: ReflectionObject::__toString() does not trigger initialization
--FILE--
<?php

class C {
    public int $a;
    public function __construct() {
        var_dump(__METHOD__);
        $this->a = 1;
    }
}

function test(string $name, object $obj) {
    printf("# %s\n", $name);

    (new ReflectionObject($obj))->__toString();

    printf("Initialized:\n");
    var_dump(!ReflectionLazyObject::fromInstance($obj));

}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct();
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function ($obj) {
    var_dump("initializer");
    return new C();
}, ReflectionLazyObject::STRATEGY_VIRTUAL);

test('Virtual', $obj);

--EXPECT--
# Ghost
Initialized:
bool(false)
# Virtual
Initialized:
bool(false)
