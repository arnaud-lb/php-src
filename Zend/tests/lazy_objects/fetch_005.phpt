--TEST--
Lazy objects: magic property fetch may not initialize object
--FILE--
<?php

class C {
    public int $a = 1;
    public function __construct() {
        var_dump(__METHOD__);
    }
    public function __get($name) {
        return $name;
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    var_dump($obj);
    var_dump($obj->magic);
    var_dump($obj);
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

--EXPECTF--
# Ghost:
object(C)#%d (0) {
  ["a"]=>
  uninitialized(int)
}
string(5) "magic"
object(C)#%d (0) {
  ["a"]=>
  uninitialized(int)
}
# Virtual:
object(C)#%d (0) {
  ["a"]=>
  uninitialized(int)
}
string(5) "magic"
object(C)#%d (0) {
  ["a"]=>
  uninitialized(int)
}
