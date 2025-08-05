--TEST--
Closure application RFC examples: delayed execution
--FILE--
<?php

class Point {}

function expensive(int $a, int $b, Point $c) { return $c; }

function some_condition() {
    return true;
}

$default = expensive(3, 4, new Point, ...);

if (some_condition()) {
  $result = $default();
  var_dump($result);
}

?>
--EXPECTF--
object(Point)#%d (0) {
}
