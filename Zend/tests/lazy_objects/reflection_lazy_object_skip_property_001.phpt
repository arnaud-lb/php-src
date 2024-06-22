--TEST--
Lazy objects: ReflectionClass::skipInitializerForProperty() prevent properties from triggering initializer
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
    $reflector = new ReflectionClass($clone);
    $skept = false;
    try {
        $propReflector->skipLazyInitialization($clone);
        $skept = true;
    } catch (ReflectionException $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }
    if ($reflector->isInitialized($obj)) {
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
        if ($reflector->isInitialized($obj)) {
            printf("Object was unexpectedly initialized (1)\n");
        }
    }

    /*
    printf("\nsetProperty():\n");
    $clone = clone $obj;
    $reflector = new ReflectionClass($clone);
    try {
        $reflector->setProperty($propReflector->getName(), 'value');
    } catch (ReflectionException $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }
    if ($reflector->isInitialized($obj)) {
        printf("Object was unexpectedly initialized (1)\n");
    }
    try {
        printf("getValue(): ");
        var_dump($getValue($clone, $propReflector));
    } catch (\Error $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }
    if ($reflector->isInitialized($obj)) {
        printf("Object was unexpectedly initialized (1)\n");
    }
     */

    printf("\nsetRawValueWithoutLazyInitialization():\n");
    $clone = clone $obj;
    $reflector = new ReflectionClass($clone);
    $skept = false;
    try {
        $propReflector->setRawValueWithoutLazyInitialization($clone, 'value');
        $skept = true;
    } catch (ReflectionException $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }
    if ($reflector->isInitialized($obj)) {
        printf("Object was unexpectedly initialized (1)\n");
    }
    if ($skept) {
        try {
            printf("getValue(): ");
            var_dump($getValue($clone, $propReflector));
        } catch (\Error $e) {
            printf("%s: %s\n", $e::class, $e->getMessage());
        }
        if ($reflector->isInitialized($obj)) {
            printf("Object was unexpectedly initialized (1)\n");
        }
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
        // TODO: refactor this test
        function skipLazyInitialization(object $object) {
            throw new \ReflectionException();
        }
        function setRawValueWithoutLazyInitialization(object $object) {
            throw new \ReflectionException();
        }
        function __toString() {
            return "Property [ \$dynamicProp ]\n";
        }
    });
}

$obj = (new ReflectionClass(B::class))->newInstanceWithoutConstructor();
(new ReflectionClass($obj))->resetAsLazyGhost($obj, function ($obj) {
    throw new \Exception('initializer');
});

test('Ghost', $obj);

$obj = (new ReflectionClass(B::class))->newInstanceWithoutConstructor();
(new ReflectionClass($obj))->resetAsLazyProxy($obj, function ($obj) {
    throw new \Exception('initializer');
});

test('Virtual', $obj);

?>
--EXPECT--
# Ghost:

## Property [ private $priv = 'privB' ]

skipInitializerForProperty():
getValue(): Error: Cannot access private property B::$priv

setRawValueWithoutLazyInitialization():
getValue(): Error: Cannot access private property B::$priv

## Property [ public $pubB = 'pubB' ]

skipInitializerForProperty():
getValue(): string(4) "pubB"

setRawValueWithoutLazyInitialization():
getValue(): string(5) "value"

## Property [ private readonly string $readonly ]

skipInitializerForProperty():
getValue(): Error: Cannot access private property B::$readonly

setRawValueWithoutLazyInitialization():
getValue(): Error: Cannot access private property B::$readonly

## Property [ protected $prot = 'prot' ]

skipInitializerForProperty():
getValue(): Error: Cannot access protected property B::$prot

setRawValueWithoutLazyInitialization():
getValue(): Error: Cannot access protected property B::$prot

## Property [ public $pubA = 'pubA' ]

skipInitializerForProperty():
getValue(): string(4) "pubA"

setRawValueWithoutLazyInitialization():
getValue(): string(5) "value"

## Property [ public static $static = 'static' ]

skipInitializerForProperty():
ReflectionException: Can not use skipLazyInitialization on static property B::$static

setRawValueWithoutLazyInitialization():
ReflectionException: Can not use setRawValueWithoutLazyInitialization on static property B::$static

## Property [ public $noDefault = NULL ]

skipInitializerForProperty():
getValue(): NULL

setRawValueWithoutLazyInitialization():
getValue(): string(5) "value"

## Property [ public string $noDefaultTyped ]

skipInitializerForProperty():
getValue(): Error: Typed property A::$noDefaultTyped must not be accessed before initialization

setRawValueWithoutLazyInitialization():
getValue(): string(5) "value"

## Property [ public $initialized = NULL ]

skipInitializerForProperty():
getValue(): NULL

setRawValueWithoutLazyInitialization():
getValue(): string(5) "value"

## Property [ public $hooked = NULL ]

skipInitializerForProperty():
getValue(): NULL

setRawValueWithoutLazyInitialization():
getValue(): string(5) "value"

## Property [ public $virtual ]

skipInitializerForProperty():
ReflectionException: Can not use skipLazyInitialization on virtual property B::$virtual

setRawValueWithoutLazyInitialization():
ReflectionException: Can not use setRawValueWithoutLazyInitialization on virtual property B::$virtual

## Property [ $dynamicProp ]

skipInitializerForProperty():
ReflectionException: 

setRawValueWithoutLazyInitialization():
ReflectionException: 
# Virtual:

## Property [ private $priv = 'privB' ]

skipInitializerForProperty():
getValue(): Error: Cannot access private property B::$priv

setRawValueWithoutLazyInitialization():
getValue(): Error: Cannot access private property B::$priv

## Property [ public $pubB = 'pubB' ]

skipInitializerForProperty():
getValue(): string(4) "pubB"

setRawValueWithoutLazyInitialization():
getValue(): string(5) "value"

## Property [ private readonly string $readonly ]

skipInitializerForProperty():
getValue(): Error: Cannot access private property B::$readonly

setRawValueWithoutLazyInitialization():
getValue(): Error: Cannot access private property B::$readonly

## Property [ protected $prot = 'prot' ]

skipInitializerForProperty():
getValue(): Error: Cannot access protected property B::$prot

setRawValueWithoutLazyInitialization():
getValue(): Error: Cannot access protected property B::$prot

## Property [ public $pubA = 'pubA' ]

skipInitializerForProperty():
getValue(): string(4) "pubA"

setRawValueWithoutLazyInitialization():
getValue(): string(5) "value"

## Property [ public static $static = 'static' ]

skipInitializerForProperty():
ReflectionException: Can not use skipLazyInitialization on static property B::$static

setRawValueWithoutLazyInitialization():
ReflectionException: Can not use setRawValueWithoutLazyInitialization on static property B::$static

## Property [ public $noDefault = NULL ]

skipInitializerForProperty():
getValue(): NULL

setRawValueWithoutLazyInitialization():
getValue(): string(5) "value"

## Property [ public string $noDefaultTyped ]

skipInitializerForProperty():
getValue(): Error: Typed property A::$noDefaultTyped must not be accessed before initialization

setRawValueWithoutLazyInitialization():
getValue(): string(5) "value"

## Property [ public $initialized = NULL ]

skipInitializerForProperty():
getValue(): NULL

setRawValueWithoutLazyInitialization():
getValue(): string(5) "value"

## Property [ public $hooked = NULL ]

skipInitializerForProperty():
getValue(): NULL

setRawValueWithoutLazyInitialization():
getValue(): string(5) "value"

## Property [ public $virtual ]

skipInitializerForProperty():
ReflectionException: Can not use skipLazyInitialization on virtual property B::$virtual

setRawValueWithoutLazyInitialization():
ReflectionException: Can not use setRawValueWithoutLazyInitialization on virtual property B::$virtual

## Property [ $dynamicProp ]

skipInitializerForProperty():
ReflectionException: 

setRawValueWithoutLazyInitialization():
ReflectionException:
