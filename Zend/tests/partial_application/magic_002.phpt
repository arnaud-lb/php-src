--TEST--
Partial application magic: __callStatic
--FILE--
<?php
class Foo {
    public static function __callStatic($method, $args) {
        printf("%s::%s\n", __CLASS__, $method);

        var_dump(...$args);
    }
}

$bar = Foo::method(?);

echo (string) new ReflectionFunction($bar);

$bar(1);

$bar = Foo::method(?, ...);

echo (string) new ReflectionFunction($bar);

$bar(10);

$bar = Foo::method(new Foo,...);

echo (string) new ReflectionFunction($bar);

$bar(100);
?>
--EXPECTF--
Partial [ <user, prototype Foo> public method method ] {
  @@ %s 10 - 10

  - Parameters [1] {
    Parameter #0 [ <required> $args ]
  }
}
Foo::method
int(1)
Partial [ <user, prototype Foo> public method method ] {
  @@ %s 16 - 16

  - Parameters [2] {
    Parameter #0 [ <required> $args ]
    Parameter #1 [ <optional> ...$args ]
  }
}
Foo::method
int(10)
Partial [ <user, prototype Foo> public method method ] {
  @@ %s 22 - 22

  - Parameters [1] {
    Parameter #0 [ <optional> ...$args ]
  }
}
Foo::method
object(Foo)#%d (0) {
}
int(100)

