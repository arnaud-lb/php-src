--TEST--
Lazy objects: unset of undefined skipped property initializes object
--FILE--
<?php

class C {
    public $a;
    public int $b = 1;
    public int $c;

    public function __construct(int $a) {
        var_dump(__METHOD__);
        $this->a = $a;
        $this->b = 2;
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    (new ReflectionLazyObjectFactory($obj))->skipProperty('a');
    (new ReflectionLazyObjectFactory($obj))->skipProperty('b');
    (new ReflectionLazyObjectFactory($obj))->skipProperty('c');

    var_dump($obj);
    unset($obj->a);
    unset($obj->b);
    unset($obj->c);
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
object(C)#%d (2) {
  ["a"]=>
  NULL
  ["b"]=>
  int(1)
  ["c"]=>
  uninitialized(int)
}
object(C)#%d (0) {
  ["b"]=>
  uninitialized(int)
  ["c"]=>
  uninitialized(int)
}
# Virtual:
object(C)#%d (2) {
  ["a"]=>
  NULL
  ["b"]=>
  int(1)
  ["c"]=>
  uninitialized(int)
}
object(C)#%d (0) {
  ["b"]=>
  uninitialized(int)
  ["c"]=>
  uninitialized(int)
}
