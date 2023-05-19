--TEST--
Lazy objects: ReflectionLazyObject::skipProperty() prevent properties from triggering initializer
--FILE--
<?php

class A {
    private $priv = 'priv A';
    private $privA = 'privA';
    protected $prot = 'prot';
    public $pubA = 'pubA';

    public static $static = 'static';

    public $noDefault;
    public string $noDefaultTyped;
    public $initialized;
}

class B extends A {
    private $priv = 'privB';
    public $pubB = 'pubB';

    private readonly string $readonly;
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    var_dump($obj);

    $lazyObj = ReflectionLazyObject::fromInstance($obj);

    printf("Init pubA\n");
    $lazyObj->skipProperty('pubA');
    var_dump($obj);

    printf("Init pubB\n");
    $lazyObj->skipProperty('pubB');
    var_dump($obj);

    printf("Init prot\n");
    $lazyObj->skipProperty('prot');
    var_dump($obj);

    printf("Init priv\n");
    $lazyObj->skipProperty('priv');
    var_dump($obj);

    printf("Init A::priv\n");
    $lazyObj->skipProperty('priv', A::class);
    var_dump($obj);

    try {
        printf("Init privA\n");
        $lazyObj->skipProperty('privA');
        var_dump($obj);
    } catch (Exception $e) {
        printf("%s\n", $e->getMessage());
    }

    printf("Init A::privA\n");
    $lazyObj->skipProperty('privA', A::class);
    var_dump($obj);

    printf("Init readonly\n");
    $lazyObj->skipProperty('readonly');
    var_dump($obj);

    printf("Init noDefault\n");
    $lazyObj->skipProperty('noDefault');
    var_dump($obj);

    printf("Init noDefaultTyped\n");
    $lazyObj->skipProperty('noDefaultTyped');
    var_dump($obj);

    printf("Init initialized\n");
    $lazyObj->setProperty('initialized', new stdClass);
    $lazyObj->skipProperty('initialized');
    var_dump($obj);

    printf("Accessing properties should not trigger initializer after skipProperty\n");
    var_dump($obj->initialized);
    var_dump($obj->noDefault);
    var_dump($obj->pubA);
    try {
        var_dump($obj->noDefaultTyped);
    } catch (Error $e) {
        printf("%s\n", $e->getMessage());
    }
}

$obj = (new ReflectionClass(B::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function ($obj) {
    var_dump("initializer");
});

test('Ghost', $obj);

$obj = (new ReflectionClass(B::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function ($obj) {
    var_dump("initializer");
    return new C();
}, ReflectionLazyObject::STRATEGY_VIRTUAL);

test('Virtual', $obj);

?>
--EXPECTF--
# Ghost:
object(B)#%d (0) {
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init pubA
object(B)#%d (1) {
  ["pubA"]=>
  string(4) "pubA"
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init pubB
object(B)#%d (2) {
  ["pubA"]=>
  string(4) "pubA"
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init prot
object(B)#%d (3) {
  ["prot":protected]=>
  string(4) "prot"
  ["pubA"]=>
  string(4) "pubA"
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init priv
object(B)#%d (4) {
  ["prot":protected]=>
  string(4) "prot"
  ["pubA"]=>
  string(4) "pubA"
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["priv":"B":private]=>
  string(5) "privB"
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init A::priv
object(B)#%d (5) {
  ["priv":"A":private]=>
  string(6) "priv A"
  ["prot":protected]=>
  string(4) "prot"
  ["pubA"]=>
  string(4) "pubA"
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["priv":"B":private]=>
  string(5) "privB"
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init privA
Property B::$privA does not exist
Init A::privA
object(B)#%d (6) {
  ["priv":"A":private]=>
  string(6) "priv A"
  ["privA":"A":private]=>
  string(5) "privA"
  ["prot":protected]=>
  string(4) "prot"
  ["pubA"]=>
  string(4) "pubA"
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["priv":"B":private]=>
  string(5) "privB"
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init readonly
object(B)#%d (6) {
  ["priv":"A":private]=>
  string(6) "priv A"
  ["privA":"A":private]=>
  string(5) "privA"
  ["prot":protected]=>
  string(4) "prot"
  ["pubA"]=>
  string(4) "pubA"
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["priv":"B":private]=>
  string(5) "privB"
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init noDefault
object(B)#%d (7) {
  ["priv":"A":private]=>
  string(6) "priv A"
  ["privA":"A":private]=>
  string(5) "privA"
  ["prot":protected]=>
  string(4) "prot"
  ["pubA"]=>
  string(4) "pubA"
  ["noDefault"]=>
  NULL
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["priv":"B":private]=>
  string(5) "privB"
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init noDefaultTyped
object(B)#%d (7) {
  ["priv":"A":private]=>
  string(6) "priv A"
  ["privA":"A":private]=>
  string(5) "privA"
  ["prot":protected]=>
  string(4) "prot"
  ["pubA"]=>
  string(4) "pubA"
  ["noDefault"]=>
  NULL
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["priv":"B":private]=>
  string(5) "privB"
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init initialized
object(B)#%d (8) {
  ["priv":"A":private]=>
  string(6) "priv A"
  ["privA":"A":private]=>
  string(5) "privA"
  ["prot":protected]=>
  string(4) "prot"
  ["pubA"]=>
  string(4) "pubA"
  ["noDefault"]=>
  NULL
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["initialized"]=>
  NULL
  ["priv":"B":private]=>
  string(5) "privB"
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Accessing properties should not trigger initializer after skipProperty
NULL
NULL
string(4) "pubA"
Typed property A::$noDefaultTyped must not be accessed before initialization
# Virtual:
object(B)#%d (0) {
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init pubA
object(B)#%d (1) {
  ["pubA"]=>
  string(4) "pubA"
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init pubB
object(B)#%d (2) {
  ["pubA"]=>
  string(4) "pubA"
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init prot
object(B)#%d (3) {
  ["prot":protected]=>
  string(4) "prot"
  ["pubA"]=>
  string(4) "pubA"
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init priv
object(B)#%d (4) {
  ["prot":protected]=>
  string(4) "prot"
  ["pubA"]=>
  string(4) "pubA"
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["priv":"B":private]=>
  string(5) "privB"
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init A::priv
object(B)#%d (5) {
  ["priv":"A":private]=>
  string(6) "priv A"
  ["prot":protected]=>
  string(4) "prot"
  ["pubA"]=>
  string(4) "pubA"
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["priv":"B":private]=>
  string(5) "privB"
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init privA
Property B::$privA does not exist
Init A::privA
object(B)#%d (6) {
  ["priv":"A":private]=>
  string(6) "priv A"
  ["privA":"A":private]=>
  string(5) "privA"
  ["prot":protected]=>
  string(4) "prot"
  ["pubA"]=>
  string(4) "pubA"
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["priv":"B":private]=>
  string(5) "privB"
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init readonly
object(B)#%d (6) {
  ["priv":"A":private]=>
  string(6) "priv A"
  ["privA":"A":private]=>
  string(5) "privA"
  ["prot":protected]=>
  string(4) "prot"
  ["pubA"]=>
  string(4) "pubA"
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["priv":"B":private]=>
  string(5) "privB"
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init noDefault
object(B)#%d (7) {
  ["priv":"A":private]=>
  string(6) "priv A"
  ["privA":"A":private]=>
  string(5) "privA"
  ["prot":protected]=>
  string(4) "prot"
  ["pubA"]=>
  string(4) "pubA"
  ["noDefault"]=>
  NULL
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["priv":"B":private]=>
  string(5) "privB"
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init noDefaultTyped
object(B)#%d (7) {
  ["priv":"A":private]=>
  string(6) "priv A"
  ["privA":"A":private]=>
  string(5) "privA"
  ["prot":protected]=>
  string(4) "prot"
  ["pubA"]=>
  string(4) "pubA"
  ["noDefault"]=>
  NULL
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["priv":"B":private]=>
  string(5) "privB"
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Init initialized
object(B)#%d (8) {
  ["priv":"A":private]=>
  string(6) "priv A"
  ["privA":"A":private]=>
  string(5) "privA"
  ["prot":protected]=>
  string(4) "prot"
  ["pubA"]=>
  string(4) "pubA"
  ["noDefault"]=>
  NULL
  ["noDefaultTyped"]=>
  uninitialized(string)
  ["initialized"]=>
  NULL
  ["priv":"B":private]=>
  string(5) "privB"
  ["pubB"]=>
  string(4) "pubB"
  ["readonly":"B":private]=>
  uninitialized(string)
}
Accessing properties should not trigger initializer after skipProperty
NULL
NULL
string(4) "pubA"
Typed property A::$noDefaultTyped must not be accessed before initialization
