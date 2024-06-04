--TEST--
Lazy objects: makeLazy() on already lazy object is not allowed
--FILE--
<?php

class C extends stdClass {
    public int $a;
}

printf("# Ghost:\n");

$obj = new C();
ReflectionLazyObject::makeLazyGhost($obj, function () {});

try {
    $r = ReflectionLazyObject::makeLazyGhost($obj, function ($obj) {
    });
} catch (\Exception $e) {
    printf("%s: %s\n", $e::class, $e->getMessage());
}

printf("# Virtual:\n");

$obj = new C();
ReflectionLazyObject::makeLazyProxy($obj, function () {});

try {
    $r = ReflectionLazyObject::makeLazyProxy($obj, function ($obj) {
    });
} catch (\Exception $e) {
    printf("%s: %s\n", $e::class, $e->getMessage());
}

$obj = new C();
ReflectionLazyObject::makeLazyProxy($obj, function () {
    return new C();
})->initialize();

try {
    $r = ReflectionLazyObject::makeLazyProxy($obj, function ($obj) {
    });
} catch (\Exception $e) {
    printf("%s: %s\n", $e::class, $e->getMessage());
}

--EXPECT--
# Ghost:
ReflectionException: Object is already lazy
# Virtual:
ReflectionException: Object is already lazy
ReflectionException: Object is already lazy
