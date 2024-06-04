--TEST--
Lazy objects: newInstanceLazy can not instantiate internal classes
--FILE--
<?php

$obj = (new ReflectionClass(ReflectionClass::class))->newInstanceWithoutConstructor();

foreach (['makeLazyGhost', 'makeLazyProxy'] as $strategy) {
    try {
        ReflectionLazyObject::$strategy($obj, function ($obj) {
            var_dump("initializer");
        });
    } catch (Error $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }
}
--EXPECT--
Error: Cannot make instance of internal class lazy: ReflectionClass is internal
Error: Cannot make instance of internal class lazy: ReflectionClass is internal
