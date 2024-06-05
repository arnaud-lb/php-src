--TEST--
Lazy objects: ReflectionLazyObjectFactory::skipProperty() prevent properties from triggering initializer
--FILE--
<?php

#[\AllowDynamicProperties]
class A {
    private $priv = 'priv A';
    private $privA = 'privA';
    protected $prot = 'prot';
    public $pubA = 'pubA';

    public static $static = 'static';

    public $noDefault;
    public string $noDefaultTyped;
    public $initialized;

    public $hooked {
        get { return $this->hooked; }
        set ($value) { $this->hooked = strtoupper($value); }
    }

    public $virtual {
        get { return 'virtual'; }
        set ($value) { }
    }
}

class B extends A {
    private $priv = 'privB';
    public $pubB = 'pubB';

    private readonly string $readonly;
}

set_error_handler(function ($errno, $errstr) {
    throw new Error($errstr);
});

function testProperty(object $obj, $propReflector) {

    $getValue = function ($obj, $propReflector) {
        $name = $propReflector->getName();
        return $obj->$name;
    };

    printf("\n## %s", $propReflector);

    printf("\nskipProperty():\n");
    $clone = clone $obj;
    $lazyReflector = new ReflectionLazyObjectFactory($clone);
    $skept = false;
    try {
        $lazyReflector->skipProperty($clone, $propReflector->getName());
        $skept = true;
    } catch (ReflectionException $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }
    if ($lazyReflector->isInitialized($obj)) {
        printf("Object was unexpectedly initialized (1)\n");
    }
    if ($skept) {
        try {
            printf("getValue(): ");
            var_dump($getValue($clone, $propReflector));
        } catch (\Error $e) {
            printf("%s: %s\n", $e::class, $e->getMessage());
        }
        if (!$propReflector->isStatic()) {
            $propReflector->setValue($clone, '');
        }
        if ($lazyReflector->isInitialized($obj)) {
            printf("Object was unexpectedly initialized (1)\n");
        }
    }

    /*
    printf("\nsetProperty():\n");
    $clone = clone $obj;
    $lazyReflector = new ReflectionLazyObjectFactory($clone);
    try {
        $lazyReflector->setProperty($propReflector->getName(), 'value');
    } catch (ReflectionException $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }
    if ($lazyReflector->isInitialized($obj)) {
        printf("Object was unexpectedly initialized (1)\n");
    }
    try {
        printf("getValue(): ");
        var_dump($getValue($clone, $propReflector));
    } catch (\Error $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }
    if ($lazyReflector->isInitialized($obj)) {
        printf("Object was unexpectedly initialized (1)\n");
    }
     */

    printf("\nsetRawProperty():\n");
    $clone = clone $obj;
    $lazyReflector = new ReflectionLazyObjectFactory($clone);
    try {
        $lazyReflector->setRawProperty($clone, $propReflector->getName(), 'value');
    } catch (ReflectionException $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }
    if ($lazyReflector->isInitialized($obj)) {
        printf("Object was unexpectedly initialized (1)\n");
    }
    try {
        printf("getValue(): ");
        var_dump($getValue($clone, $propReflector));
    } catch (\Error $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }
    if ($lazyReflector->isInitialized($obj)) {
        printf("Object was unexpectedly initialized (1)\n");
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    $reflector = new ReflectionClass($obj::class);

    foreach ($reflector->getProperties() as $propReflector) {
        testProperty($obj, $propReflector);
    }

    testProperty($obj, new class {
        function getName() {
            return 'dynamicProp';
        }
        function setValue($obj, $value) {
            $obj->dynamicProp = $value;
        }
        function isStatic() {
            return false;
        }
        function __toString() {
            return "Property [ \$dynamicProp ]\n";
        }
    });
}

$obj = (new ReflectionClass(B::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
});

test('Ghost', $obj);

$obj = (new ReflectionClass(B::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    return new A();
});

test('Virtual', $obj);

?>
--EXPECT--
# Ghost:

## Property [ private $priv = 'privB' ]

skipProperty():
getValue(): Error: Cannot access private property B::$priv

setRawProperty():
getValue(): Error: Cannot access private property B::$priv

## Property [ public $pubB = 'pubB' ]

skipProperty():
getValue(): string(4) "pubB"

setRawProperty():
getValue(): string(5) "value"

## Property [ private readonly string $readonly ]

skipProperty():
getValue(): Error: Cannot access private property B::$readonly

setRawProperty():
getValue(): Error: Cannot access private property B::$readonly

## Property [ protected $prot = 'prot' ]

skipProperty():
getValue(): Error: Cannot access protected property B::$prot

setRawProperty():
getValue(): Error: Cannot access protected property B::$prot

## Property [ public $pubA = 'pubA' ]

skipProperty():
getValue(): string(4) "pubA"

setRawProperty():
getValue(): string(5) "value"

## Property [ public static $static = 'static' ]

skipProperty():
ReflectionException: Can not initialize static property B::$static

setRawProperty():
ReflectionException: Can not use setRawProperty on static property B::$static
getValue(): Error: Accessing static property B::$static as non static

## Property [ public $noDefault = NULL ]

skipProperty():
getValue(): NULL

setRawProperty():
getValue(): string(5) "value"

## Property [ public string $noDefaultTyped ]

skipProperty():
getValue(): Error: Typed property A::$noDefaultTyped must not be accessed before initialization

setRawProperty():
getValue(): string(5) "value"

## Property [ public $initialized = NULL ]

skipProperty():
getValue(): NULL

setRawProperty():
getValue(): string(5) "value"

## Property [ public $hooked = NULL ]

skipProperty():
getValue(): NULL

setRawProperty():
getValue(): string(5) "value"

## Property [ public $virtual ]

skipProperty():
ReflectionException: Can not initialize virtual property B::$virtual

setRawProperty():
ReflectionException: Can not use setRawProperty on virtual property B::$virtual
getValue(): string(7) "virtual"

## Property [ $dynamicProp ]

skipProperty():
ReflectionException: Property B::$dynamicProp does not exist

setRawProperty():
getValue(): string(5) "value"
# Virtual:

## Property [ private $priv = 'privB' ]

skipProperty():
getValue(): Error: Cannot access private property B::$priv

setRawProperty():
getValue(): Error: Cannot access private property B::$priv

## Property [ public $pubB = 'pubB' ]

skipProperty():
getValue(): string(4) "pubB"

setRawProperty():
getValue(): string(5) "value"

## Property [ private readonly string $readonly ]

skipProperty():
getValue(): Error: Cannot access private property B::$readonly

setRawProperty():
getValue(): Error: Cannot access private property B::$readonly

## Property [ protected $prot = 'prot' ]

skipProperty():
getValue(): Error: Cannot access protected property B::$prot

setRawProperty():
getValue(): Error: Cannot access protected property B::$prot

## Property [ public $pubA = 'pubA' ]

skipProperty():
getValue(): string(4) "pubA"

setRawProperty():
getValue(): string(5) "value"

## Property [ public static $static = 'static' ]

skipProperty():
ReflectionException: Can not initialize static property B::$static

setRawProperty():
ReflectionException: Can not use setRawProperty on static property B::$static
getValue(): Error: Accessing static property B::$static as non static

## Property [ public $noDefault = NULL ]

skipProperty():
getValue(): NULL

setRawProperty():
getValue(): string(5) "value"

## Property [ public string $noDefaultTyped ]

skipProperty():
getValue(): Error: Typed property A::$noDefaultTyped must not be accessed before initialization

setRawProperty():
getValue(): string(5) "value"

## Property [ public $initialized = NULL ]

skipProperty():
getValue(): NULL

setRawProperty():
getValue(): string(5) "value"

## Property [ public $hooked = NULL ]

skipProperty():
getValue(): NULL

setRawProperty():
getValue(): string(5) "value"

## Property [ public $virtual ]

skipProperty():
ReflectionException: Can not initialize virtual property B::$virtual

setRawProperty():
ReflectionException: Can not use setRawProperty on virtual property B::$virtual
getValue(): string(7) "virtual"

## Property [ $dynamicProp ]

skipProperty():
ReflectionException: Property B::$dynamicProp does not exist

setRawProperty():
getValue(): string(5) "value"
