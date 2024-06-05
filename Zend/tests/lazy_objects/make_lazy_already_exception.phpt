--TEST--
Lazy objects: makeLazy() on already lazy object is not allowed
--FILE--
<?php

class C extends stdClass {
    public int $a;
}

printf("# Ghost:\n");

$obj = new C();
ReflectionLazyObjectFactory::makeLazyGhost($obj, function () {});

try {
    $r = ReflectionLazyObjectFactory::makeLazyGhost($obj, function ($obj) {
    });
} catch (\Exception $e) {
    printf("%s: %s\n", $e::class, $e->getMessage());
}

printf("# Virtual:\n");

$obj = new C();
ReflectionLazyObjectFactory::makeLazyProxy($obj, function () {});

try {
    $r = ReflectionLazyObjectFactory::makeLazyProxy($obj, function ($obj) {
    });
} catch (\Exception $e) {
    printf("%s: %s\n", $e::class, $e->getMessage());
}

$obj = new C();
ReflectionLazyObjectFactory::makeLazyProxy($obj, function () {
    return new C();
})->initialize($obj);

try {
    $r = ReflectionLazyObjectFactory::makeLazyProxy($obj, function ($obj) {
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
