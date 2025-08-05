--TEST--
Closure application clone
--FILE--
<?php

class C {
    public $a;
    public $b;
}

$clone = clone(?);
var_dump($clone(new C));

$clone = clone(...);
var_dump($clone(new C));

$clone = clone(new C, ?);
var_dump($clone(['a' => 1]));

$clone = clone(?, ['a' => 1]);
var_dump($clone(new C));

?>
--EXPECTF--
object(C)#%d (2) {
  ["a"]=>
  NULL
  ["b"]=>
  NULL
}
object(C)#%d (2) {
  ["a"]=>
  NULL
  ["b"]=>
  NULL
}
object(C)#%d (2) {
  ["a"]=>
  int(1)
  ["b"]=>
  NULL
}
object(C)#%d (2) {
  ["a"]=>
  int(1)
  ["b"]=>
  NULL
}
