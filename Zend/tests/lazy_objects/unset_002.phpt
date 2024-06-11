--TEST--
Lazy objects: unset of defined dynamic property does not initialize object
--FILE--
<?php

#[AllowDynamicProperties]
class C {
    public int $b = 1;

    public function __construct(int $a) {
        var_dump(__METHOD__);
        $this->b = 2;
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    (new ReflectionLazyObjectFactory($obj))->setRawPropertyValue($obj, 'a', 1);

    var_dump($obj);
    unset($obj->a);
    var_dump($obj);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeInstanceLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct(1);
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeInstanceLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    return new C(1);
});

test('Virtual', $obj);

--EXPECTF--
# Ghost:
object(C)#%d (1) {
  ["b"]=>
  uninitialized(int)
  ["a"]=>
  int(1)
}
object(C)#%d (0) {
  ["b"]=>
  uninitialized(int)
}
# Virtual:
object(C)#%d (1) {
  ["b"]=>
  uninitialized(int)
  ["a"]=>
  int(1)
}
object(C)#%d (0) {
  ["b"]=>
  uninitialized(int)
}
