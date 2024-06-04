--TEST--
Lazy objects: newInstanceLazy can instantiate stdClass
--FILE--
<?php

foreach (['makeLazyGhost', 'makeLazyProxy'] as $strategy) {
    $obj = (new ReflectionClass(stdClass::class))->newInstanceWithoutConstructor();
    ReflectionLazyObject::$strategy($obj, function ($obj) {
        var_dump("initializer");
    });
    var_dump($obj);
}
--EXPECTF--
object(stdClass)#%d (0) {
}
object(stdClass)#%d (0) {
}
