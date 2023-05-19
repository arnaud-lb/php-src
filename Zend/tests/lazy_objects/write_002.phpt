--TEST--
Lazy objects: property write to custom property initializes object
--FILE--
<?php

#[AllowDynamicProperties]
class C {
    public $a;
    public int $b = 1;

    public function __construct() {
        var_dump(__METHOD__);
        $this->a = 1;
        $this->b = 2;
    }
}
function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    var_dump($obj);
    $obj->custom = 3;
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
  ["b"]=>
  uninitialized(int)
}
string(11) "initializer"
string(14) "C::__construct"
object(C)#%d (3) {
  ["a"]=>
  int(1)
  ["b"]=>
  int(2)
  ["custom"]=>
  int(3)
}
# Virtual:
object(C)#%d (0) {
  ["b"]=>
  uninitialized(int)
}
string(11) "initializer"
string(14) "C::__construct"
object(C)#%d (1) {
  ["instance"]=>
  object(C)#%d (3) {
    ["a"]=>
    int(1)
    ["b"]=>
    int(2)
    ["custom"]=>
    int(3)
  }
}
