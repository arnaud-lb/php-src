--TEST--
Lazy objects: newInstanceLazy can not instantiate sub-class of internal classes
--FILE--
<?php

class C extends ReflectionClass {}

foreach ([ReflectionLazyObject::STRATEGY_GHOST, ReflectionLazyObject::STRATEGY_VIRTUAL] as $strategy) {
    $obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
    try {
        if ($strategy === ReflectionLazyObject::STRATEGY_GHOST) {
            ReflectionLazyObject::makeLazyGhost($obj, function ($obj) {
                var_dump("initializer");
            });
        } else {
            ReflectionLazyObject::makeLazyProxy($obj, function ($obj) {
                var_dump("initializer");
            });
        }
    } catch (Error $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }
}
--EXPECT--
Error: Cannot make instance of internal class lazy: C inherits internal class ReflectionClass
Error: Cannot make instance of internal class lazy: C inherits internal class ReflectionClass
