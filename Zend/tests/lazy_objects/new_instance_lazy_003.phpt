--TEST--
Lazy objects: newInstanceLazy can instantiate sub-class of user classes
--FILE--
<?php

class B {}
class C extends B {}

foreach (['makeInstanceLazyGhost', 'makeInstanceLazyProxy'] as $strategy) {
    $obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
    ReflectionLazyObjectFactory::$strategy($obj, function ($obj) {
        var_dump("initializer");
    });
    var_dump($obj);
}

--EXPECTF--
object(C)#%d (0) {
}
object(C)#%d (0) {
}
