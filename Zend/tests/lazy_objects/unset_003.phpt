--TEST--
Lazy objects: unset of magic property may not initialize object
--FILE--
<?php

class C {
    public int $b = 1;

    public function __construct(int $a) {
        var_dump(__METHOD__);
        $this->b = 2;
    }

    public function __unset($name) {
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    var_dump($obj);
    unset($obj->a);
    var_dump($obj);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct(1);
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    return new C(1);
});

test('Virtual', $obj);

--EXPECTF--
# Ghost:
object(C)#%d (0) {
  ["b"]=>
  uninitialized(int)
}
object(C)#%d (0) {
  ["b"]=>
  uninitialized(int)
}
# Virtual:
object(C)#%d (0) {
  ["b"]=>
  uninitialized(int)
}
object(C)#%d (0) {
  ["b"]=>
  uninitialized(int)
}
