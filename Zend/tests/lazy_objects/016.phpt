--TEST--
Lazy objects: destructor of lazy objets is not called if not initialized
--FILE--
<?php

class C {
    public $a;
    public function __destruct() {
        var_dump(__METHOD__);
    }
}

print "# Ghost:\n";

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function () {
    var_dump("initializer");
});

print "# Virtual:\n";

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function () {
    var_dump("initializer");
}, ReflectionLazyObject::STRATEGY_VIRTUAL);

--EXPECTF--
# Ghost:
# Virtual:
