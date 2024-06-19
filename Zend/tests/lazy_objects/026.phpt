--TEST--
Lazy objects: var_dump may initialize object with __debugInfo() method
--FILE--
<?php

class C {
    public int $a;
    public function __construct() {
        var_dump(__METHOD__);
        $this->a = 1;
    }
    public function __debugInfo() {
        return [$this->a];
    }
}

function test(string $name, object $obj) {
    printf("# %s\n", $name);

    var_dump($obj);
    printf("Initialized:\n");
    var_dump((new ReflectionClass($obj))->isInitialized($obj));
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
(new ReflectionClass($obj))->resetAsLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct();
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
(new ReflectionClass($obj))->resetAsLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    return new C();
});

test('Virtual', $obj);

--EXPECTF--
# Ghost
string(11) "initializer"
string(14) "C::__construct"
object(C)#%d (1) {
  [0]=>
  int(1)
}
Initialized:
bool(true)
# Virtual
string(11) "initializer"
string(14) "C::__construct"
object(C)#%d (1) {
  [0]=>
  int(1)
}
Initialized:
bool(true)
