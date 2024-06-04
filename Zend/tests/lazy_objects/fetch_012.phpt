--TEST--
Lazy objects: property op error
--FILE--
<?php

class C {
    public int $a = 1;
    public function __construct() {
        var_dump(__METHOD__);
        $this->a = 2;
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    var_dump($obj);

    try {
        var_dump($obj->a++);
    } catch (Error $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }

    var_dump($obj);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyGhost($obj, function ($obj) {
    throw new Error("initializer");
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyProxy($obj, function ($obj) {
    throw new Error("initializer");
});

test('Virtual', $obj);

--EXPECTF--
# Ghost:
object(C)#%d (0) {
  ["a"]=>
  uninitialized(int)
}
Error: initializer
object(C)#%d (0) {
  ["a"]=>
  uninitialized(int)
}
# Virtual:
object(C)#%d (0) {
  ["a"]=>
  uninitialized(int)
}
Error: initializer
object(C)#%d (0) {
  ["a"]=>
  uninitialized(int)
}
