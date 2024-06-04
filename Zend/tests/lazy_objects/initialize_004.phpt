--TEST--
Lazy objects: ReflectionLazyObject::initialize with custom initializer from scope
--XFAIL--
initialize() takes a boolean
--FILE--
<?php

class C {
    public int $a;
}

class D {
    public function initialize($obj) {
        $reflector = ReflectionLazyObject::fromInstance($obj);
        $reflector->initialize($this->initializer(...));
    }

    private function initializer($obj) {
        var_dump("custom initializer");
        $obj->a = 2;
    }
}

class E {
    public function initialize($obj) {
        $reflector = ReflectionLazyObject::fromInstance($obj);
        $reflector->initialize($this->initializer(...));
    }

    private function initializer($obj) {
        var_dump("custom initializer");
        $c = new C();
        $c->a = 2;
        return $c;
    }
}

function test(string $name, object $obj, object $initializer) {
    printf("# %s:\n", $name);

    $reflector = ReflectionLazyObject::fromInstance($obj);

    var_dump($reflector->isInitialized());

    $initializer->initialize($obj);

    var_dump($reflector->isInitialized());
    var_dump($obj);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->a = 1;
});

test('Ghost', $obj, new D());

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    $c = new C();
    $c->a = 1;
    return $c;
});

test('Virtual', $obj, new E());

--EXPECTF--
# Ghost:
bool(false)
string(18) "custom initializer"
bool(true)
object(C)#%d (1) {
  ["a"]=>
  int(2)
}
# Virtual:
bool(false)
string(18) "custom initializer"
bool(true)
object(C)#%d (1) {
  ["instance"]=>
  object(C)#%d (1) {
    ["a"]=>
    int(2)
  }
}
