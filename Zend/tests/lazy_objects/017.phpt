--TEST--
Lazy objects: destructor of initialized objets is called
--FILE--
<?php

class C {
    public int $a = 1;

    public function __destruct() {
        var_dump(__METHOD__, $this);
    }
}

function ghost() {
    print "# Ghost:\n";

    $obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
    ReflectionLazyObject::makeLazy($obj, function () {
        var_dump("initializer");
    });

    var_dump($obj->a);
}

function virtual() {
    print "# Virtual:\n";

    $obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
    ReflectionLazyObject::makeLazy($obj, function () {
        var_dump("initializer");
        return new C();
    }, ReflectionLazyObject::STRATEGY_VIRTUAL);

    var_dump($obj->a);
}

ghost();
virtual();

--EXPECTF--
# Ghost:
string(11) "initializer"
int(1)
string(13) "C::__destruct"
object(C)#%d (1) {
  ["a"]=>
  int(1)
}
# Virtual:
string(11) "initializer"
int(1)
string(13) "C::__destruct"
object(C)#%d (1) {
  ["a"]=>
  int(1)
}
