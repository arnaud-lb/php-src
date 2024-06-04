--TEST--
Lazy objects: write to skipped property does not initialize object
--FILE--
<?php

class C {
    public $a;
    public int $b = 1;
    public int $c;

    public function __construct() {
        var_dump(__METHOD__);
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    ReflectionLazyObject::fromInstance($obj)->skipProperty('a');
    ReflectionLazyObject::fromInstance($obj)->skipProperty('b');
    ReflectionLazyObject::fromInstance($obj)->skipProperty('c');

    var_dump($obj);
    $obj->a = 2;
    $obj->b = 2;
    $obj->c = 2;
    var_dump($obj);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct();
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyVirtual($obj, function ($obj) {
    var_dump("initializer");
    return new C();
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
object(C)#%d (3) {
  ["a"]=>
  int(2)
  ["b"]=>
  int(2)
  ["c"]=>
  int(2)
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
object(C)#%d (3) {
  ["a"]=>
  int(2)
  ["b"]=>
  int(2)
  ["c"]=>
  int(2)
}
