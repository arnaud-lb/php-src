--TEST--
PFA with $this placeholder - sub class with renamed params
--FILE--
<?php

class C {
    public function f($a = null, $b = null) {
        die("Not called");
    }
}

class SubC extends C {
    public function f($c = null, $d = null) {
        printf("%s(%s)::%s(%s, %s)\n", __CLASS__, get_class($this), __FUNCTION__, $c, $d);
    }
}

$f = C::f(this: ?, ...);
echo new ReflectionFunction($f), "\n";
$f(__this: new SubC(), a: 'a', b: 'b');

?>
--EXPECTF--
Closure [ <user> static public method {closure:pfa:%s:15} ] {
  @@ %s 15 - 15

  - Parameters [3] {
    Parameter #0 [ <required> C $__this ]
    Parameter #1 [ <optional> $a = NULL ]
    Parameter #2 [ <optional> $b = NULL ]
  }
}

SubC(SubC)::f(a, b)
