--TEST--
Lazy objects: get_object_vars does not initialize object
--FILE--
<?php

class C {
    public int $a;
    public function __construct() {
        var_dump(__METHOD__);
        $this->a = 1;
    }
}

print "# Ghost:\n";

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct();
});

var_dump(get_object_vars($obj));

$obj->a = 2;
var_dump(get_object_vars($obj));

print "# Virtual:\n";

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    return new C();
});

var_dump(get_object_vars($obj));

$obj->a = 2;
var_dump(get_object_vars($obj));

--EXPECTF--
# Ghost:
array(0) {
}
string(11) "initializer"
string(14) "C::__construct"
array(1) {
  ["a"]=>
  int(2)
}
# Virtual:
array(0) {
}
string(11) "initializer"
string(14) "C::__construct"
array(1) {
  ["a"]=>
  int(2)
}
