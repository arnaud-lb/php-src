--TEST--
Lazy objects: property write with initializer exception
--FILE--
<?php

class C {
    public $a;
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    var_dump($obj);
    try {
        $obj->a = 1;
    } catch (Exception $e) {
        printf("%s\n", $e->getMessage());
    }
    var_dump($obj);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyGhost($obj, function ($obj) {
    throw new \Exception('init exception');
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyVirtual($obj, function ($obj) {
    throw new \Exception('init exception');
});

test('Ghost', $obj);

--EXPECTF--
# Ghost:
object(C)#%d (0) {
}
init exception
object(C)#%d (0) {
}
# Ghost:
object(C)#%d (0) {
}
init exception
object(C)#%d (0) {
}
