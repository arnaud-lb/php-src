--TEST--
Closure application invokable
--FILE--
<?php

class C {
    public function __invoke(int $a, object $b): C {
        var_dump($a, $b);
        return $this;
    }
}

$c = new C();
$f = $c(?, ...);

echo (string) new ReflectionFunction($f), "\n";

$f = $c(?, new stdClass);

echo (string) new ReflectionFunction($f), "\n";

$f(1);

?>
--EXPECTF--
Closure [ <user, prototype C> public method {closure:%s} ] {
  @@ %s.php 11 - 11

  - Parameters [2] {
    Parameter #0 [ <required> int $a ]
    Parameter #1 [ <required> object $b ]
  }
  - Return [ C ]
}

Closure [ <user, prototype C> public method {closure:%s} ] {
  @@ %s.php 15 - 15

  - Parameters [1] {
    Parameter #0 [ <required> int $a ]
  }
  - Return [ C ]
}

int(1)
object(stdClass)#%d (0) {
}
