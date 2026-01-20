--TEST--
PFA with $this placeholder - name conflict
--FILE--
<?php

class C {
    function f($__this = null, $b = null) {
        printf("%s::%s(%s, %s)\n", get_class($this), __FUNCTION__, $__this, $b);
    }
}

$f = C::f($this: ?, ...);
echo new ReflectionFunction($f), "\n";
$f(new C(), 'a', 'b');

?>
--EXPECTF--
Closure [ <user> static public method {closure:pfa:%s:9} ] {
  @@ %s 9 - 9

  - Parameters [3] {
    Parameter #0 [ <required> C $__this2 ]
    Parameter #1 [ <optional> $__this = NULL ]
    Parameter #2 [ <optional> $b = NULL ]
  }
}

C::f(a, b)
