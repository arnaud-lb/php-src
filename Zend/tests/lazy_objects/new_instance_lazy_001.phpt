--TEST--
Lazy objects: newInstanceLazy can not instantiate internal classes
--FILE--
<?php

$obj = (new ReflectionClass(ReflectionClass::class))->newInstanceWithoutConstructor();

foreach ([ReflectionLazyObject::STRATEGY_GHOST, ReflectionLazyObject::STRATEGY_VIRTUAL] as $strategy) {
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
Error: Cannot make instance of internal class lazy: ReflectionClass is internal
Error: Cannot make instance of internal class lazy: ReflectionClass is internal
