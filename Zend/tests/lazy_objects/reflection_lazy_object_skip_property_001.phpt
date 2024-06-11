--TEST--
Lazy objects: ReflectionLazyObjectFactory::skipInitializerForProperty() prevent properties from triggering initializer
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

    printf("\nskipInitializerForProperty():\n");
    $clone = clone $obj;
    $lazyReflector = new ReflectionLazyObjectFactory($clone);
    $skept = false;
    try {
        $lazyReflector->skipInitializerForProperty($clone, $propReflector->getName());
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

    printf("\nsetRawPropertyValue():\n");
    $clone = clone $obj;
    $lazyReflector = new ReflectionLazyObjectFactory($clone);
    try {
        $lazyReflector->setRawPropertyValue($clone, $propReflector->getName(), 'value');
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
ReflectionLazyObjectFactory::makeInstanceLazyGhost($obj, function ($obj) {
    var_dump("initializer");
});

test('Ghost', $obj);

$obj = (new ReflectionClass(B::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeInstanceLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    return new A();
});

test('Virtual', $obj);

?>
--EXPECT--
# Ghost:

## Property [ private $priv = 'privB' ]

skipInitializerForProperty():
getValue(): Error: Cannot access private property B::$priv

setRawPropertyValue():
getValue(): Error: Cannot access private property B::$priv

## Property [ public $pubB = 'pubB' ]

skipInitializerForProperty():
getValue(): string(4) "pubB"

setRawPropertyValue():
getValue(): string(5) "value"

## Property [ private readonly string $readonly ]

skipInitializerForProperty():
getValue(): Error: Cannot access private property B::$readonly

setRawPropertyValue():
getValue(): Error: Cannot access private property B::$readonly

## Property [ protected $prot = 'prot' ]

skipInitializerForProperty():
getValue(): Error: Cannot access protected property B::$prot

setRawPropertyValue():
getValue(): Error: Cannot access protected property B::$prot

## Property [ public $pubA = 'pubA' ]

skipInitializerForProperty():
getValue(): string(4) "pubA"

setRawPropertyValue():
getValue(): string(5) "value"

## Property [ public static $static = 'static' ]

skipInitializerForProperty():
ReflectionException: Can not initialize static property B::$static

setRawPropertyValue():
ReflectionException: Can not use setRawPropertyValue on static property B::$static
getValue(): Error: Accessing static property B::$static as non static

## Property [ public $noDefault = NULL ]

skipInitializerForProperty():
getValue(): NULL

setRawPropertyValue():
getValue(): string(5) "value"

## Property [ public string $noDefaultTyped ]

skipInitializerForProperty():
getValue(): Error: Typed property A::$noDefaultTyped must not be accessed before initialization

setRawPropertyValue():
getValue(): string(5) "value"

## Property [ public $initialized = NULL ]

skipInitializerForProperty():
getValue(): NULL

setRawPropertyValue():
getValue(): string(5) "value"

## Property [ public $hooked = NULL ]

skipInitializerForProperty():
getValue(): NULL

setRawPropertyValue():
getValue(): string(5) "value"

## Property [ public $virtual ]

skipInitializerForProperty():
ReflectionException: Can not initialize virtual property B::$virtual

setRawPropertyValue():
ReflectionException: Can not use setRawPropertyValue on virtual property B::$virtual
getValue(): string(7) "virtual"

## Property [ $dynamicProp ]

skipInitializerForProperty():
ReflectionException: Property B::$dynamicProp does not exist

setRawPropertyValue():
getValue(): string(5) "value"
# Virtual:

## Property [ private $priv = 'privB' ]

skipInitializerForProperty():
getValue(): Error: Cannot access private property B::$priv

setRawPropertyValue():
getValue(): Error: Cannot access private property B::$priv

## Property [ public $pubB = 'pubB' ]

skipInitializerForProperty():
getValue(): string(4) "pubB"

setRawPropertyValue():
getValue(): string(5) "value"

## Property [ private readonly string $readonly ]

skipInitializerForProperty():
getValue(): Error: Cannot access private property B::$readonly

setRawPropertyValue():
getValue(): Error: Cannot access private property B::$readonly

## Property [ protected $prot = 'prot' ]

skipInitializerForProperty():
getValue(): Error: Cannot access protected property B::$prot

setRawPropertyValue():
getValue(): Error: Cannot access protected property B::$prot

## Property [ public $pubA = 'pubA' ]

skipInitializerForProperty():
getValue(): string(4) "pubA"

setRawPropertyValue():
getValue(): string(5) "value"

## Property [ public static $static = 'static' ]

skipInitializerForProperty():
ReflectionException: Can not initialize static property B::$static

setRawPropertyValue():
ReflectionException: Can not use setRawPropertyValue on static property B::$static
getValue(): Error: Accessing static property B::$static as non static

## Property [ public $noDefault = NULL ]

skipInitializerForProperty():
getValue(): NULL

setRawPropertyValue():
getValue(): string(5) "value"

## Property [ public string $noDefaultTyped ]

skipInitializerForProperty():
getValue(): Error: Typed property A::$noDefaultTyped must not be accessed before initialization

setRawPropertyValue():
getValue(): string(5) "value"

## Property [ public $initialized = NULL ]

skipInitializerForProperty():
getValue(): NULL

setRawPropertyValue():
getValue(): string(5) "value"

## Property [ public $hooked = NULL ]

skipInitializerForProperty():
getValue(): NULL

setRawPropertyValue():
getValue(): string(5) "value"

## Property [ public $virtual ]

skipInitializerForProperty():
ReflectionException: Can not initialize virtual property B::$virtual

setRawPropertyValue():
ReflectionException: Can not use setRawPropertyValue on virtual property B::$virtual
getValue(): string(7) "virtual"

## Property [ $dynamicProp ]

skipInitializerForProperty():
ReflectionException: Property B::$dynamicProp does not exist

setRawPropertyValue():
getValue(): string(5) "value"
