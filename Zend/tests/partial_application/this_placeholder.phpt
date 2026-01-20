--TEST--
PFA with $this placeholder basics
--FILE--
<?php

class C {
    function f($a = null, $b = null) {
        printf("%s::%s(%s, %s)\n", get_class($this), __FUNCTION__, $a, $b);
    }
}

echo "# Basics\n";

$f = C::f($this: ?, ?, ?);
echo new ReflectionFunction($f), "\n";
$f(new C, 'a', 'b');

echo "\n# \$this: can be used anywhere, and makes anything before it required\n";

$f = C::f(?, $this: ?, ?);
echo new ReflectionFunction($f), "\n";
$f('a', new C, 'b');

echo "\n# \$this: can be used anywhere, and makes anything before it required (2)\n";

$f = C::f(?, ?, $this: ?);
echo new ReflectionFunction($f), "\n";
$f('a', 'b', new C);

echo "\n# \$this: can be used anywhere (variadic placeholder)\n";

$f = C::f($this: ?, ...);
echo new ReflectionFunction($f), "\n";
$f(new C, 'a', 'b');

echo "\n# \$this: can be used anywhere (names params)\n";

$f = C::f(b: ?, $this: ?, a: ?);
echo new ReflectionFunction($f), "\n";
$f('b', new C, 'a');

?>
--EXPECTF--
# Basics
Closure [ <user> static public method {closure:pfa:%s:11} ] {
  @@ %s 11 - 11

  - Parameters [3] {
    Parameter #0 [ <required> C $__this ]
    Parameter #1 [ <optional> $a = NULL ]
    Parameter #2 [ <optional> $b = NULL ]
  }
}

C::f(a, b)

# $this: can be used anywhere, and makes anything before it required
Closure [ <user> static public method {closure:pfa:%s:17} ] {
  @@ %s 17 - 17

  - Parameters [3] {
    Parameter #0 [ <required> $a ]
    Parameter #1 [ <required> C $__this ]
    Parameter #2 [ <optional> $b = NULL ]
  }
}

C::f(a, b)

# $this: can be used anywhere, and makes anything before it required (2)
Closure [ <user> static public method {closure:pfa:%s:23} ] {
  @@ %s 23 - 23

  - Parameters [3] {
    Parameter #0 [ <required> $a ]
    Parameter #1 [ <required> $b ]
    Parameter #2 [ <required> C $__this ]
  }
}

C::f(a, b)

# $this: can be used anywhere (variadic placeholder)
Closure [ <user> static public method {closure:pfa:%s:29} ] {
  @@ %s 29 - 29

  - Parameters [3] {
    Parameter #0 [ <required> C $__this ]
    Parameter #1 [ <optional> $a = NULL ]
    Parameter #2 [ <optional> $b = NULL ]
  }
}

C::f(a, b)

# $this: can be used anywhere (names params)
Closure [ <user> static public method {closure:pfa:%s:35} ] {
  @@ %s 35 - 35

  - Parameters [3] {
    Parameter #0 [ <required> $b ]
    Parameter #1 [ <required> C $__this ]
    Parameter #2 [ <optional> $a = NULL ]
  }
}

C::f(a, b)
