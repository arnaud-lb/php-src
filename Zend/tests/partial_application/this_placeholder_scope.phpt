--TEST--
PFA with $this placeholder - scope
--FILE--
<?php

class C {
    public function get_f() {
        return C::f(this: ?, ...);
    }
    private function f($a = null, $b = null) {
        printf("%s::%s(%s, %s)\n", get_class($this), __FUNCTION__, $a, $b);
    }
}

$f = new C()->get_f();
echo new ReflectionFunction($f), "\n";
$f(new C(), 'a', 'b');

?>
--EXPECTF--
Closure [ <user> static public method {closure:pfa:C::get_f():5} ] {
  @@ %s 5 - 5

  - Parameters [3] {
    Parameter #0 [ <required> C $__this ]
    Parameter #1 [ <optional> $a = NULL ]
    Parameter #2 [ <optional> $b = NULL ]
  }
}

C::f(a, b)
