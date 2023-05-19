--TEST--
Lazy objects: var_dump does not initialize object
--FILE--
<?php

class C {
    public int $a;
    public function __construct() {
        var_dump(__METHOD__);
        $this->a = 1;
    }
}

print "# Ghost:\n";

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct();
});

var_dump($obj);

print "# Virtual:\n";

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function ($obj) {
    var_dump("initializer");
    return new C();
}, ReflectionLazyObject::STRATEGY_VIRTUAL);

var_dump($obj);

--EXPECTF--
# Ghost:
object(C)#%d (0) {
  ["a"]=>
  uninitialized(int)
}
# Virtual:
object(C)#%d (0) {
  ["a"]=>
  uninitialized(int)
}
