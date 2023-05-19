--TEST--
Lazy objects: newInstanceLazy can instantiate sub-class of stdClass
--FILE--
<?php

class C extends stdClass {}

foreach ([ReflectionLazyObject::STRATEGY_GHOST, ReflectionLazyObject::STRATEGY_VIRTUAL] as $flags) {
    $obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
    ReflectionLazyObject::makeLazy($obj, function ($obj) {
        var_dump("initializer");
    });
    var_dump($obj);
}

--EXPECTF--
object(C)#%d (0) {
}
object(C)#%d (0) {
}
