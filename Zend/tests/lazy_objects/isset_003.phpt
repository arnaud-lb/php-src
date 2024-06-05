--TEST--
Lazy objects: hooked property isset may not initialize object
--FILE--
<?php

class C {
    public $a {
        get { return 1; }
        set($value) { }
    }
    public int $b = 1;

    public function __construct(int $a) {
        var_dump(__METHOD__);
        $this->a = $a;
        $this->b = 2;
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    var_dump($obj);
    var_dump(isset($obj->a));
    var_dump($obj);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct(1);
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeLazyProxy($obj, function ($obj) {
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
bool(true)
object(C)#%d (0) {
  ["b"]=>
  uninitialized(int)
}
# Virtual:
object(C)#%d (0) {
  ["b"]=>
  uninitialized(int)
}
bool(true)
object(C)#%d (0) {
  ["b"]=>
  uninitialized(int)
}
