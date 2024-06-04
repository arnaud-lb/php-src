--TEST--
Lazy objects: newInstanceLazy can instantiate sub-class of stdClass
--FILE--
<?php

class C extends stdClass {}

foreach (['makeLazyGhost', 'makeLazyProxy'] as $strategy) {
    $obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
    ReflectionLazyObject::$strategy($obj, function ($obj) {
        var_dump("initializer");
    });
    var_dump($obj);
}

--EXPECTF--
object(C)#%d (0) {
}
object(C)#%d (0) {
}
