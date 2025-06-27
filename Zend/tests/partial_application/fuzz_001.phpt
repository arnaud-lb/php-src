--TEST--
Partial application fuzz 001
--FILE--
<?php
$closure = function($a, $b) {};

echo (string) new ReflectionFunction($closure(1, ?));
?>
--EXPECTF--
Partial [ <user> function {closure:%s:%d} ] {
  @@ %s 4 - 4

  - Parameters [1] {
    Parameter #0 [ <required> $b ]
  }
}
