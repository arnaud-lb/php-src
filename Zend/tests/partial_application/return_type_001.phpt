--TEST--
Closure application return type
--FILE--
<?php
function foo($a) : array {}

echo (string) new ReflectionFunction(foo(new stdClass, ...));
?>
--EXPECTF--
Closure [ <user> function {closure:%s} ] {
  @@ %s 4 - 4

  - Parameters [0] {
  }
  - Return [ array ]
}

