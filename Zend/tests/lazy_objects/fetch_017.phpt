--TEST--
Lazy objects: virtual hooked property fetch may initialize object
--FILE--
<?php

class C {
    public $_a;
    public $a {
        &get { return $this->_a; }
    }
    public int $b = 1;

    public function __construct(int $a) {
        var_dump(__METHOD__);
        $this->_a = $a;
        $this->b = 2;
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    var_dump($obj);
    $a = &$obj->a;
    var_dump($a);
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
string(11) "initializer"
string(14) "C::__construct"
int(1)
object(C)#%d (2) {
  ["_a"]=>
  &int(1)
  ["b"]=>
  int(2)
}
# Virtual:
object(C)#%d (0) {
  ["b"]=>
  uninitialized(int)
}
string(11) "initializer"
string(14) "C::__construct"
int(1)
object(C)#%d (1) {
  ["instance"]=>
  object(C)#%d (2) {
    ["_a"]=>
    &int(1)
    ["b"]=>
    int(2)
  }
}
