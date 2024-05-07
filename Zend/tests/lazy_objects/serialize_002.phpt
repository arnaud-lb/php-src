--TEST--
Lazy objects: serialize initializes object by default (properties hashtable)
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
    printf("# %s:\n", $name);

    // Builds properties hashtable
    get_object_vars($obj);

    var_dump(serialize($obj));
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
# Ghost:
string(11) "initializer"
string(14) "C::__construct"
string(24) "O:1:"C":1:{s:1:"a";i:1;}"
# Virtual:
string(11) "initializer"
string(14) "C::__construct"
string(24) "O:1:"C":1:{s:1:"a";i:1;}"
