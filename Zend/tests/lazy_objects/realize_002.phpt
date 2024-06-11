--TEST--
Lazy objects: Object is not lazy anymore if all props have been skipped
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

    $reflector = new ReflectionLazyObjectFactory($obj);
    var_dump($reflector->isInitialized($obj));

    $reflector->skipInitializerForProperty($obj, 'a');
    var_dump($reflector->isInitialized($obj));

    // Should not count a second prop initialization
    $reflector->skipInitializerForProperty($obj, 'a');
    var_dump($reflector->isInitialized($obj));

    try {
        // Should not count a prop initialization
        $reflector->skipInitializerForProperty($obj, 'xxx');
    } catch (ReflectionException $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }

    try {
        // Should not count a prop initialization
        $reflector->skipInitializerForProperty($obj, 'b');
    } catch (ReflectionException $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }

    $reflector->skipInitializerForProperty($obj, 'b', B::class);
    var_dump($reflector->isInitialized($obj));

    var_dump($obj);
}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeInstanceLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct();
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeInstanceLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    return new C();
});

test('Virtual', $obj);

--EXPECT--
# Ghost:
bool(false)
bool(false)
bool(false)
ReflectionException: Property C::$xxx does not exist
ReflectionException: Property C::$b does not exist
bool(true)
object(C)#2 (0) {
  ["b":"B":private]=>
  uninitialized(string)
  ["a"]=>
  uninitialized(string)
}
# Virtual:
bool(false)
bool(false)
bool(false)
ReflectionException: Property C::$xxx does not exist
ReflectionException: Property C::$b does not exist
bool(true)
object(C)#3 (0) {
  ["b":"B":private]=>
  uninitialized(string)
  ["a"]=>
  uninitialized(string)
}
