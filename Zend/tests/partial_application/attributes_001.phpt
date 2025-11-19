--TEST--
Closure application copies attributes
--XFAIL--
Only NoDiscard and SensitiveParameter are copied
--FILE--
<?php

#[Attribute]
class Test {}

#[NoDiscard]
function f($a, #[SensitiveParameter] $b, #[Test] ...$c) {
}

function dump_attributes($function) {
    echo "Function attributes:\n";
    $r = new ReflectionFunction($function);
    var_dump($r->getAttributes());

    foreach ($r->getParameters() as $i => $p) {
        echo "Parameter $i:\n";
        var_dump($p->getAttributes());
    }
}

dump_attributes('f');

$f = f(1, ?, ?, ...);

dump_attributes($f);

?>
--EXPECTF--
Function attributes:
array(1) {
  [0]=>
  object(ReflectionAttribute)#%d (1) {
    ["name"]=>
    string(9) "NoDiscard"
  }
}
Parameter 0:
array(0) {
}
Parameter 1:
array(1) {
  [0]=>
  object(ReflectionAttribute)#%d (1) {
    ["name"]=>
    string(18) "SensitiveParameter"
  }
}
Parameter 2:
array(1) {
  [0]=>
  object(ReflectionAttribute)#%d (1) {
    ["name"]=>
    string(4) "Test"
  }
}
Function attributes:
array(1) {
  [0]=>
  object(ReflectionAttribute)#%d (1) {
    ["name"]=>
    string(9) "NoDiscard"
  }
}
Parameter 0:
array(1) {
  [0]=>
  object(ReflectionAttribute)#%d (1) {
    ["name"]=>
    string(18) "SensitiveParameter"
  }
}
Parameter 1:
array(1) {
  [0]=>
  object(ReflectionAttribute)#%d (1) {
    ["name"]=>
    string(4) "Test"
  }
}
Parameter 2:
array(1) {
  [0]=>
  object(ReflectionAttribute)#%d (1) {
    ["name"]=>
    string(4) "Test"
  }
}
