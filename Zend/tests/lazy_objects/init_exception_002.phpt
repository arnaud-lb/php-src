--TEST--
Lazy objects: Initializer effects are reverted after exception
--FILE--
<?php

class C {
    public $a = 1;
    public int $b = 2;
    public int $c;
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    (new ReflectionLazyObjectFactory($obj))->setRawPropertyValue($obj, 'c', 0);

    try {
        ReflectionLazyObjectFactory::initialize($obj);
    } catch (Exception $e) {
        printf("%s\n", $e->getMessage());
    }

    var_dump($obj);
    printf("Is lazy: %d\n", !ReflectionLazyObjectFactory::isInitialized($obj));
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeInstanceLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 3;
    $obj->b = 4;
    $obj->c = 5;
    throw new Exception('initializer exception');
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeInstanceLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 3;
    $obj->b = 4;
    $obj->c = 5;
    throw new Exception('initializer exception');
});

// Initializer effects on the virtual proxy are not reverted
test('Virtual', $obj);

--EXPECTF--
# Ghost:
string(11) "initializer"
initializer exception
object(C)#%d (1) {
  ["b"]=>
  uninitialized(int)
  ["c"]=>
  int(0)
}
Is lazy: 1
# Virtual:
string(11) "initializer"
initializer exception
object(C)#%d (3) {
  ["a"]=>
  int(3)
  ["b"]=>
  int(4)
  ["c"]=>
  int(5)
}
Is lazy: 1
