--TEST--
Partial application variation debug user
--FILE--
<?php
function bar($a = 1, $b = 2, ...$c) {

}

var_dump(bar(?, new stdClass, 20, new stdClass, four: 4));
?>
--EXPECTF--
object(Closure)#%d (5) {
  ["name"]=>
  string(3) "bar"
  ["file"]=>
  string(83) "%svariation_debug_001.php"
  ["line"]=>
  int(6)
  ["parameter"]=>
  array(1) {
    ["$a"]=>
    string(10) "<optional>"
  }
  ["args"]=>
  array(3) {
    ["a"]=>
    NULL
    ["b"]=>
    object(stdClass)#%d (0) {
    }
    ["c"]=>
    array(3) {
      [0]=>
      int(20)
      [1]=>
      object(stdClass)#%d (0) {
      }
      ["four"]=>
      int(4)
    }
  }
}
