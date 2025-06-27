--TEST--
Partial application reflection: required parameters
--FILE--
<?php
function foo($a = 1, $b = 5, $c = 10) {

}

$foo = foo(?, ...);

echo (string) new ReflectionFunction($foo);

$foo = foo(?, ?, ...);

echo (string) new ReflectionFunction($foo);

$foo = foo(?, ?, ?);

echo (string) new ReflectionFunction($foo);
?>
--EXPECTF--
Partial [ <user> function foo ] {
  @@ %sreflection_001.php 6 - 6

  - Parameters [3] {
    Parameter #0 [ <optional> $a ]
    Parameter #1 [ <optional> $b ]
    Parameter #2 [ <optional> $c ]
  }
}
Partial [ <user> function foo ] {
  @@ %sreflection_001.php 10 - 10

  - Parameters [3] {
    Parameter #0 [ <optional> $a ]
    Parameter #1 [ <optional> $b ]
    Parameter #2 [ <optional> $c ]
  }
}
Partial [ <user> function foo ] {
  @@ %sreflection_001.php 14 - 14

  - Parameters [3] {
    Parameter #0 [ <optional> $a ]
    Parameter #1 [ <optional> $b ]
    Parameter #2 [ <optional> $c ]
  }
}
