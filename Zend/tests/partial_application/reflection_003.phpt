--TEST--
Partial application reflection: internal with variadics
--FILE--
<?php
$foo = sprintf(?);

echo (string) new ReflectionFunction($foo);

$foo = sprintf(?, ...);

echo (string) new ReflectionFunction($foo);

$foo = sprintf(?, ?);

echo (string) new ReflectionFunction($foo);
?>
--EXPECTF--
Partial [ <user> function sprintf ] {
  @@ %sreflection_003.php 2 - 2

  - Parameters [1] {
    Parameter #0 [ <required> string $format ]
  }
  - Return [ string ]
}
Partial [ <user> function sprintf ] {
  @@ %sreflection_003.php 6 - 6

  - Parameters [2] {
    Parameter #0 [ <required> string $format ]
    Parameter #1 [ <optional> mixed ...$values ]
  }
  - Return [ string ]
}
Partial [ <user> function sprintf ] {
  @@ %sreflection_003.php 10 - 10

  - Parameters [2] {
    Parameter #0 [ <required> string $format ]
    Parameter #1 [ <required> mixed $ ]
  }
  - Return [ string ]
}
