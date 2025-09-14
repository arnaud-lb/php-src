--TEST--
Closure application variation closure
--FILE--
<?php
$closure = function($a, $b) {

};

echo (string) new ReflectionFunction($closure(1, ?));
?>
--EXPECTF--
Closure [ <user> static function {closure:%s:%d} ] {
  @@ %s 6 - 6

  - Bound Variables [2] {
      Variable #0 [ $a ]
      Variable #1 [ $fn ]
  }

  - Parameters [1] {
    Parameter #0 [ <required> $b ]
  }
}
