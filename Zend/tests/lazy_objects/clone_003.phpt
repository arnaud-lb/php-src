--TEST--
Lazy objects: __clone may trigger initialization
--FILE--
<?php

class C {
    public $a = 1;

    public function __construct() {
    }

    public function __clone() {
        $this->a++;
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    $clone = clone $obj;

    var_dump(!(new ReflectionClass($obj))->isInitialized($obj));
    var_dump($obj);
    var_dump(!(new ReflectionClass($clone))->isInitialized($clone));
    var_dump($clone);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
(new ReflectionClass($obj))->resetAsLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct();
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
(new ReflectionClass($obj))->resetAsLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    return new C();
});

test('Virtual', $obj);

--EXPECTF--
# Ghost:
string(11) "initializer"
bool(true)
object(C)#%d (0) {
}
bool(false)
object(C)#%d (1) {
  ["a"]=>
  int(2)
}
# Virtual:
string(11) "initializer"
bool(true)
object(C)#%d (0) {
}
bool(false)
object(C)#%d (1) {
  ["instance"]=>
  object(C)#%d (1) {
    ["a"]=>
    int(2)
  }
}
