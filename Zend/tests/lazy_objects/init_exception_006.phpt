--TEST--
Lazy objects: Initializer effects are reverted after exception (dynamic properties, initialized hashtable)
--FILE--
<?php

#[AllowDynamicProperties]
class C {
    public $a = 1;
    public int $b = 2;
    public int $c;
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    // Builds properties hashtable
    var_dump(get_object_vars($obj));

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
    $obj->d = 6;
    throw new Exception('initializer exception');
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeInstanceLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 3;
    $obj->b = 4;
    $obj->c = 5;
    $obj->d = 6;
    throw new Exception('initializer exception');
});

// Initializer effects on the virtual proxy are not reverted
test('Virtual', $obj);

--EXPECTF--
# Ghost:
array(0) {
}
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
array(0) {
}
string(11) "initializer"
initializer exception
object(C)#%d (4) {
  ["a"]=>
  int(3)
  ["b"]=>
  int(4)
  ["c"]=>
  int(5)
  ["d"]=>
  int(6)
}
Is lazy: 1
