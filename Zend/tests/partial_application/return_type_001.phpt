--TEST--
Partial application return type
--FILE--
<?php
function foo($a) : array {}

echo (string) new ReflectionFunction(foo(new stdClass, ...));
?>
--EXPECTF--
Partial [ <user> function foo ] {
  @@ %s 4 - 4

  - Parameters [0] {
  }
  - Return [ array ]
}

