--TEST--
Lazy objects: GC 001
--FILE--
<?php

class Canary {
    public function __destruct() {
        var_dump(__FUNCTION__);
    }
}

class C {
}

function ghost() {
    printf("# Ghost:\n");
    $canary = new Canary();

    $obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
    ReflectionLazyObject::makeLazyGhost($obj, function () use ($canary) {
    });

    $canary = null;
    $obj = null;

    gc_collect_cycles();
}

function virtual() {
    printf("# Virtual:\n");
    $canary = new Canary();

    $obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
    ReflectionLazyObject::makeLazyVirtual($obj, function () use ($canary) {
        return new C();
    });

    $canary = null;
    $obj = null;

    gc_collect_cycles();
}

ghost();
virtual();

?>
==DONE==
--EXPECT--
# Ghost:
string(10) "__destruct"
# Virtual:
string(10) "__destruct"
==DONE==
