--TEST--
Lazy objects: dymamic properties are unset when object is made lazy
--FILE--
<?php

class Canary {
    public function __destruct() {
        var_dump(__METHOD__);
    }
}

#[\AllowDynamicProperties]
class C {
    public $b;
    public function __construct() {
        $this->a = new Canary();
    }
}

print "# Ghost:\n";

$obj = new C();
ReflectionLazyObjectFactory::makeInstanceLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct();
});

var_dump($obj);
var_dump($obj->a);
var_dump($obj);

print "# Virtual:\n";

$obj = new C();
ReflectionLazyObjectFactory::makeInstanceLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    return new C();
});

var_dump($obj);
var_dump($obj->a);
var_dump($obj->a);
var_dump($obj);

--EXPECTF--
# Ghost:
string(18) "Canary::__destruct"
object(C)#%d (0) {
}
string(11) "initializer"
object(Canary)#%d (0) {
}
object(C)#%d (2) {
  ["b"]=>
  NULL
  ["a"]=>
  object(Canary)#%d (0) {
  }
}
# Virtual:
string(18) "Canary::__destruct"
string(18) "Canary::__destruct"
object(C)#%d (0) {
}
string(11) "initializer"
object(Canary)#%d (0) {
}
object(Canary)#%d (0) {
}
object(C)#%d (1) {
  ["instance"]=>
  object(C)#%d (2) {
    ["b"]=>
    NULL
    ["a"]=>
    object(Canary)#%d (0) {
    }
  }
}
string(18) "Canary::__destruct"
