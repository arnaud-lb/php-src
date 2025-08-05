--TEST--
Closure application magic null ptr deref in arginfo
--FILE--
<?php
class Foo {
    function __call($name, $args) {
        var_dump($args);
    }
}
$foo = new Foo;
$bar = $foo->method(?);
var_dump($bar);
?>
--EXPECTF--
object(Closure)#%d (6) {
  ["name"]=>
  string(%d) "{closure:%s}"
  ["file"]=>
  string(73) "%smagic_005.php"
  ["line"]=>
  int(8)
  ["this"]=>
  object(Foo)#%d (0) {
  }
  ["parameter"]=>
  array(1) {
    ["$"]=>
    string(10) "<required>"
  }
  ["args"]=>
  array(1) {
    ["args"]=>
    array(1) {
      [0]=>
      NULL
    }
  }
}
