--TEST--
Closure application magic: __call
--FILE--
<?php
class Foo {
    public function __call($method, $args) {
        printf("%s::%s\n", __CLASS__, $method);

        var_dump(...$args);
    }
}

$foo = new Foo;

$bar = $foo->method(?);

echo (string) new ReflectionFunction($bar);

try {
    $bar();
} catch (Error $ex) {
    printf("%s\n", $ex->getMessage());
}

try {
    $bar(1, 2);
} catch (Error $ex) {
    printf("%s\n", $ex->getMessage());
}

$bar(1);

$bar = $foo->method(?, ...);

echo (string) new ReflectionFunction($bar);

$bar(10);

$bar = $foo->method(new Foo, ...);

echo (string) new ReflectionFunction($bar);

$bar(100);
?>
--EXPECTF--
Closure [ <user, prototype Foo> public method {closure:%s} ] {
  @@ %s 12 - 12

  - Parameters [1] {
    Parameter #0 [ <required> $ ]
  }
}
not enough arguments for application of Foo::method, 0 given and exactly 1 expected, declared in %s on line 12
too many arguments for application of Foo::method, 2 given and a maximum of 1 expected, declared in %s on line 12
Foo::method
int(1)
Closure [ <user, prototype Foo> public method {closure:%s} ] {
  @@ %s 30 - 30

  - Parameters [2] {
    Parameter #0 [ <required> $ ]
    Parameter #1 [ <optional> ...$args ]
  }
}
Foo::method
int(10)
Closure [ <user, prototype Foo> public method {closure:%s} ] {
  @@ %s 36 - 36

  - Parameters [1] {
    Parameter #0 [ <optional> ...$args ]
  }
}
Foo::method
object(Foo)#%d (0) {
}
int(100)

