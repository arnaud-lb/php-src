--TEST--
Closure application variation closure
--FILE--
<?php
$closure = function($a, $b) {

};

echo (string) new ReflectionFunction($closure(1, ?));
?>
--EXPECTF--
Closure [ <user> function {closure:%s} ] {
  @@ %s 6 - 6

  - Parameters [1] {
    Parameter #0 [ <required> $b ]
  }
}
