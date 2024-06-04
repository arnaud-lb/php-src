--TEST--
Lazy objects: final classes can be initialized lazily
--FILE--
<?php

final class C {
    public int $a;
    public function __construct() {
        var_dump(__METHOD__);
        $this->a = 1;
    }
}

print "# Ghost:\n";

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct();
});

var_dump($obj);
var_dump($obj->a);
var_dump($obj);

print "# Virtual:\n";

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyVirtual($obj, function ($obj) {
    var_dump("initializer");
    return new C();
});

var_dump($obj);
var_dump($obj->a);
var_dump($obj);

--EXPECTF--
# Ghost:
object(C)#%d (0) {
  ["a"]=>
  uninitialized(int)
}
string(11) "initializer"
string(14) "C::__construct"
int(1)
object(C)#%d (1) {
  ["a"]=>
  int(1)
}
# Virtual:
object(C)#%d (0) {
  ["a"]=>
  uninitialized(int)
}
string(11) "initializer"
string(14) "C::__construct"
int(1)
object(C)#%d (1) {
  ["instance"]=>
  object(C)#%d (1) {
    ["a"]=>
    int(1)
  }
}
