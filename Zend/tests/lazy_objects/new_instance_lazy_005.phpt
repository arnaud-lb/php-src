--TEST--
Lazy objects: newInstanceLazy can instantiate sub-class of stdClass
--FILE--
<?php

class C extends stdClass {}

foreach ([ReflectionLazyObject::STRATEGY_GHOST, ReflectionLazyObject::STRATEGY_VIRTUAL] as $strategy) {
    $obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
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
object(C)#%d (0) {
}
object(C)#%d (0) {
}
