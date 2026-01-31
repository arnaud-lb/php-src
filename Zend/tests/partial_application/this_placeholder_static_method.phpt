--TEST--
PFA with $this placeholder - static method
--FILE--
<?php

class C {
    public static function f($a = null, $b = null) {
        printf("%s::%s(%s, %s)\n", __CLASS__, __FUNCTION__, $a, $b);
    }
}

$f = C::f(this: ?, ...);
echo new ReflectionFunction($f), "\n";
$f(new C(), 'a', 'b');

?>
--EXPECTF--
Closure [ <user> static public method {closure:pfa:%s:9} ] {
  @@ %s 9 - 9

  - Parameters [3] {
    Parameter #0 [ <required> C $__this ]
    Parameter #1 [ <optional> $a = NULL ]
    Parameter #2 [ <optional> $b = NULL ]
  }
}

C::f(a, b)
