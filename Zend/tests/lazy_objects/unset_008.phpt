--TEST--
Lazy objects: unset of defined property does not initialize object
--FILE--
<?php

#[AllowDynamicProperties]
class C {
    public int $a = 1;

    public function __construct(int $a) {
        var_dump(__METHOD__);
        $this->a = 2;
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    ReflectionLazyObject::fromInstance($obj)->setProperty('a', 1);

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
ReflectionLazyObject::makeLazyVirtual($obj, function ($obj) {
    var_dump("initializer");
    return new C(1);
});

test('Virtual', $obj);

--EXPECTF--
# Ghost:
object(C)#%d (1) {
  ["a"]=>
  int(1)
}
object(C)#%d (0) {
  ["a"]=>
  uninitialized(int)
}
# Virtual:
object(C)#%d (1) {
  ["a"]=>
  int(1)
}
object(C)#%d (0) {
  ["a"]=>
  uninitialized(int)
}
