--TEST--
Lazy objects: Object is not lazy anymore if all props have been assigned a value
--FILE--
<?php

#[AllowDynamicProperties]
class B {
    private readonly string $b;

    public function __construct() {
        $this->b = 'b';
    }
}

#[AllowDynamicProperties]
class C extends B {
    public string $a;

    public function __construct() {
        parent::__construct();
        $this->a = 'a';
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    $reflector = ReflectionLazyObject::fromInstance($obj);
    var_dump($reflector->isInitialized());

    $reflector->setProperty('a', 'a1');
    var_dump($reflector->isInitialized());

    // Should not count a second prop initialization
    $reflector->setProperty('a', 'a2');
    var_dump($reflector->isInitialized());

    try {
        // Should not count a prop initialization
        $reflector->setProperty('a', new stdClass);
    } catch (Error $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }

    // Should not count a prop initialization
    $reflector->setProperty('b', 'dynamic B');
    var_dump($reflector->isInitialized());

    $reflector->setProperty('b', 'b', B::class);
    var_dump($reflector->isInitialized());

    var_dump($obj);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct();
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazyVirtual($obj, function ($obj) {
    var_dump("initializer");
    return new C();
});

test('Virtual', $obj);

--EXPECT--
# Ghost:
bool(false)
bool(false)
bool(false)
TypeError: Cannot assign stdClass to property C::$a of type string
bool(false)
bool(true)
object(C)#2 (3) {
  ["b":"B":private]=>
  string(1) "b"
  ["a"]=>
  string(2) "a2"
  ["b"]=>
  string(9) "dynamic B"
}
# Virtual:
bool(false)
bool(false)
bool(false)
TypeError: Cannot assign stdClass to property C::$a of type string
bool(false)
bool(true)
object(C)#4 (3) {
  ["b":"B":private]=>
  string(1) "b"
  ["a"]=>
  string(2) "a2"
  ["b"]=>
  string(9) "dynamic B"
}
