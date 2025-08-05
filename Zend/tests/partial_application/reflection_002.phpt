--TEST--
Closure application reflection: variadics
--FILE--
<?php
function foo($a, ...$b) {
    var_dump(func_get_args());
}

$foo = foo(?);

echo (string) new ReflectionFunction($foo);

$foo = foo(?, ...);

echo (string) new ReflectionFunction($foo);

$foo = foo(?, ?);

echo (string) new ReflectionFunction($foo);

$foo = foo(?, ?, ?);

echo (string) new ReflectionFunction($foo);
?>
--EXPECTF--
Closure [ <user> function {closure:%s} ] {
  @@ %s 6 - 6

  - Parameters [1] {
    Parameter #0 [ <required> $a ]
  }
}
Closure [ <user> function {closure:%s} ] {
  @@ %s 10 - 10

  - Parameters [2] {
    Parameter #0 [ <required> $a ]
    Parameter #1 [ <optional> ...$b ]
  }
}
Closure [ <user> function {closure:%s} ] {
  @@ %s 14 - 14

  - Parameters [2] {
    Parameter #0 [ <required> $a ]
    Parameter #1 [ <required> $b0 ]
  }
}
Closure [ <user> function {closure:%s} ] {
  @@ %s 18 - 18

  - Parameters [3] {
    Parameter #0 [ <required> $a ]
    Parameter #1 [ <required> $b0 ]
    Parameter #2 [ <required> $b1 ]
  }
}
