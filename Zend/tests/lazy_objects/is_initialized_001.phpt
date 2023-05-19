--TEST--
Lazy objects: ReflectionLazyObject::isInitialized
--FILE--
<?php

class C {
    public int $a;
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 1;
});

$reflector = ReflectionLazyObject::fromInstance($obj);
var_dump($reflector?->isInitialized());

var_dump($obj->a);

var_dump($reflector?->isInitialized());
--EXPECTF--
bool(false)
string(11) "initializer"
int(1)
bool(true)
