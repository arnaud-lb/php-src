--TEST--
PFA with $this placeholder - interface
--FILE--
<?php

interface MyInterface {
    public function method(string $foo): int|string|bool;
}

class ParentClass implements MyInterface {
    public function method(string|array $foo): string {
        return sprintf("%s(%s)", __METHOD__, $foo);
    }
}

class ChildClass extends ParentClass {
    public function method(string|array $bar): string {
        return sprintf("%s(%s)", __METHOD__, $bar);
    }
}

$i = MyInterface::method(this: ?, ...);
echo new ReflectionFunction($i), "\n";
var_dump($i(new ParentClass(), 'a'));
var_dump($i(new ChildClass(), 'a'));

?>
--EXPECTF--
Closure [ <user> static public method {closure:pfa:%s:19} ] {
  @@ %s 19 - 19

  - Parameters [2] {
    Parameter #0 [ <required> MyInterface $__this ]
    Parameter #1 [ <required> string $foo ]
  }
  - Return [ string|int|bool ]
}

string(22) "ParentClass::method(a)"
string(21) "ChildClass::method(a)"
