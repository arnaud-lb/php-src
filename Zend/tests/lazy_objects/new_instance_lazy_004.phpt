--TEST--
Lazy objects: newInstanceLazy can instantiate stdClass
--FILE--
<?php

foreach ([ReflectionLazyObject::STRATEGY_GHOST, ReflectionLazyObject::STRATEGY_VIRTUAL] as $flags) {
    $obj = (new ReflectionClass(stdClass::class))->newInstanceWithoutConstructor();
    ReflectionLazyObject::makeLazy($obj, function ($obj) {
        var_dump("initializer");
    });
    var_dump($obj);
}
--EXPECTF--
object(stdClass)#%d (0) {
}
object(stdClass)#%d (0) {
}
