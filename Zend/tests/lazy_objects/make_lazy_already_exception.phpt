--TEST--
Lazy objects: makeLazy() on already lazy object is not allowed
--FILE--
<?php

class C extends stdClass {
    public int $a;
}

printf("# Ghost:\n");

$obj = new C();
ReflectionLazyObject::makeLazy($obj, function () {});

try {
    $r = ReflectionLazyObject::makeLazy($obj, function ($obj) {
    }, ReflectionLazyObject::STRATEGY_GHOST);
} catch (\Exception $e) {
    printf("%s: %s\n", $e::class, $e->getMessage());
}

printf("# Virtual:\n");

$obj = new C();
ReflectionLazyObject::makeLazy($obj, function () {});

try {
    $r = ReflectionLazyObject::makeLazy($obj, function ($obj) {
    }, ReflectionLazyObject::STRATEGY_VIRTUAL);
} catch (\Exception $e) {
    printf("%s: %s\n", $e::class, $e->getMessage());
}

$obj = new C();
ReflectionLazyObject::makeLazy($obj, function () {
    return new C();
}, ReflectionLazyObject::STRATEGY_VIRTUAL)->initialize();

try {
    $r = ReflectionLazyObject::makeLazy($obj, function ($obj) {
    }, ReflectionLazyObject::STRATEGY_VIRTUAL);
} catch (\Exception $e) {
    printf("%s: %s\n", $e::class, $e->getMessage());
}

--EXPECT--
# Ghost:
ReflectionException: Object is already lazy
# Virtual:
ReflectionException: Object is already lazy
ReflectionException: Object is already lazy
