--TEST--
Partial application named placeholder
--FILE--
<?php

class A {}
class B {}
class C {}

function foo($a = 1, $b = 2, $c = 3) {
    var_dump($a, $b, $c);
}

$foo = foo(b: ?);

echo (string) new ReflectionFunction($foo);

$foo(new B);

$foo = $foo(?);

echo (string) new ReflectionFunction($foo);

$foo(new B);

$foo = foo(?, ?);
$foo = $foo(b: ?);

echo (string) new ReflectionFunction($foo);

$foo(new B);

function bar($a = 1, $b = 2, ...$c) {
    var_dump($a, $b, $c);
}

$bar = bar(..., b: ?);

echo (string) new ReflectionFunction($bar);

$bar(new A, new B, new C);

try {
    $bar = $bar(?, a: ?);
} catch (\Throwable $e) {
    echo $e->getMessage(), "\n";
}

try {
    $bar = $bar(..., c: ?);
} catch (\Throwable $e) {
    echo $e->getMessage(), "\n";
}

?>
--EXPECTF--
Partial [ <user> function foo ] {
  @@ %snamed_placeholder_001.php 11 - 11

  - Parameters [1] {
    Parameter #0 [ <optional> $b = 2 ]
  }
}
int(1)
object(B)#%d (0) {
}
int(3)
Partial [ <user> function foo ] {
  @@ %snamed_placeholder_001.php 17 - 17

  - Parameters [1] {
    Parameter #0 [ <optional> $b = 2 ]
  }
}
int(1)
object(B)#%d (0) {
}
int(3)
Partial [ <user> function foo ] {
  @@ %snamed_placeholder_001.php 24 - 24

  - Parameters [1] {
    Parameter #0 [ <optional> $b = 2 ]
  }
}
int(1)
object(B)#%d (0) {
}
int(3)
Partial [ <user> function bar ] {
  @@ %snamed_placeholder_001.php 34 - 34

  - Parameters [3] {
    Parameter #0 [ <optional> $a = 1 ]
    Parameter #1 [ <optional> $b = 2 ]
    Parameter #2 [ <optional> ...$c ]
  }
}
object(A)#%d (0) {
}
object(B)#%d (0) {
}
array(1) {
  [0]=>
  object(C)#%d (0) {
  }
}
Named parameter $a overwrites previous placeholder
Cannot use named placeholder for unknown or variadic parameter $c
