--TEST--
Closure application fuzz 001
--FILE--
<?php
$closure = function($a, $b) {};

echo (string) new ReflectionFunction($closure(1, ?));
?>
--EXPECTF--
Closure [ <user> function {closure:%s} ] {
  @@ %s 4 - 4

  - Parameters [1] {
    Parameter #0 [ <required> $b ]
  }
}
