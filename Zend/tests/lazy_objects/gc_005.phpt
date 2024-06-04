--TEST--
Lazy objects: GC 005
--FILE--
<?php

class Canary {
    public $value;
    public function __destruct() {
        var_dump(__FUNCTION__);
    }
}

class C {
    public $value;
}

function ghost() {
    printf("# Ghost:\n");

    $canary = new Canary();

    $obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
    $reflector = ReflectionLazyObject::makeLazyGhost($obj, function () use ($canary) {
    });

    $reflector->setProperty('value', $obj);
    $reflector = null;

    $canary->value = $obj;
    $obj = null;
    $canary = null;

    gc_collect_cycles();
}

function virtual() {
    printf("# Virtual:\n");

    $canary = new Canary();

    $obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
    $reflector = ReflectionLazyObject::makeLazyVirtual($obj, function () use ($canary) {
        return new C();
    });

    $reflector->setProperty('value', $obj);
    $reflector = null;

    $canary->value = $obj;
    $obj = null;
    $canary = null;

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
