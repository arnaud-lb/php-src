--TEST--
Lazy objects: ReflectionLazyObjectFactory::initialize with custom initializer
--XFAIL--
initialize() takes a boolean
--FILE--
<?php

class C {
    public int $a;
}

function test(string $name, object $obj, callable $initializer) {
    printf("# %s:\n", $name);

    $reflector = new ReflectionLazyObjectFactory($obj);

    var_dump($reflector->isInitialized($obj));

    var_dump($reflector->initialize($initializer));

    var_dump($reflector->isInitialized($obj));
    var_dump($obj);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 1;
});

test('Ghost', $obj, function ($obj) {
    var_dump("custom initializer");
    $obj->a = 2;
});

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    $c = new C();
    $c->a = 1;
    return $c;
});

test('Virtual', $obj, function ($obj) {
    var_dump("custom initializer");
    $c = new C();
    $c->a = 2;
    return $c;
});

--EXPECTF--
# Ghost:
bool(false)
string(18) "custom initializer"
object(C)#2 (1) {
  ["a"]=>
  int(2)
}
bool(true)
object(C)#%d (1) {
  ["a"]=>
  int(2)
}
# Virtual:
bool(false)
string(18) "custom initializer"
object(C)#5 (1) {
  ["a"]=>
  int(2)
}
bool(true)
object(C)#%d (1) {
  ["instance"]=>
  object(C)#%d (1) {
    ["a"]=>
    int(2)
  }
}
