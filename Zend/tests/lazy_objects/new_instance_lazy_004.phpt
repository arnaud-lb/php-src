--TEST--
Lazy objects: newInstanceLazy can instantiate stdClass
--FILE--
<?php

foreach ([ReflectionLazyObject::STRATEGY_GHOST, ReflectionLazyObject::STRATEGY_VIRTUAL] as $strategy) {
    $obj = (new ReflectionClass(stdClass::class))->newInstanceWithoutConstructor();
    if ($strategy === ReflectionLazyObject::STRATEGY_GHOST) {
        ReflectionLazyObject::makeLazyGhost($obj, function ($obj) {
            var_dump("initializer");
        });
    } else {
        ReflectionLazyObject::makeLazyProxy($obj, function ($obj) {
            var_dump("initializer");
        });
    }
    var_dump($obj);
}
--EXPECTF--
object(stdClass)#%d (0) {
}
object(stdClass)#%d (0) {
}
