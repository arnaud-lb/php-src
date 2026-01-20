--TEST--
PFA with $this placeholder - __call()
--FILE--
<?php

class C {
    public function __call($name, $args) {
        printf("%s::%s(%s, %s)\n", get_class($this), __FUNCTION__, $name, $args[0]);
    }
    public static function __callStatic($name, $args) {
        die("Not called");
    }
}

class D {
    public function __call($name, $args) {
        printf("%s::%s(%s, %s)\n", __CLASS__, __FUNCTION__, $name, $args[0]);
    }
}

$f = C::f($this: ?, ...);
echo new ReflectionFunction($f), "\n";
$f(new C(), 'a', 'b');

$f = D::f($this: ?, ...);
echo new ReflectionFunction($f), "\n";
$f(new D(), 'a', 'b');

?>
--EXPECTF--
Closure [ <user> static public method {closure:pfa:%s:18} ] {
  @@ %s 18 - 18

  - Parameters [2] {
    Parameter #0 [ <required> C $__this ]
    Parameter #1 [ <optional> mixed ...$arguments ]
  }
}

C::__call(f, a)
Closure [ <user> static public method {closure:pfa:%s:22} ] {
  @@ %s 22 - 22

  - Parameters [2] {
    Parameter #0 [ <required> D $__this ]
    Parameter #1 [ <optional> mixed ...$arguments ]
  }
}

D::__call(f, a)
