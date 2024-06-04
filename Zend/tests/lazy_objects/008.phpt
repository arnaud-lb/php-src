--TEST--
Lazy objects: var_export initializes object
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
ReflectionLazyObject::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct();
});

var_export($obj);
print "\n";

print "# Virtual:\n";

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    return new C();
});

var_export($obj);
print "\n";
--EXPECTF--
# Ghost:
string(11) "initializer"
string(14) "C::__construct"
\C::__set_state(array(
   'a' => 1,
))
# Virtual:
string(11) "initializer"
string(14) "C::__construct"
\C::__set_state(array(
   'a' => 1,
))
