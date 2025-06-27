--TEST--
Partial application variation closure
--FILE--
<?php
$closure = function($a, $b) {

};

echo (string) new ReflectionFunction($closure(1, ?));
?>
--EXPECTF--
Partial [ <user> function {closure:%s:%d} ] {
  @@ %s 6 - 6

  - Parameters [1] {
    Parameter #0 [ <required> $b ]
  }
}
