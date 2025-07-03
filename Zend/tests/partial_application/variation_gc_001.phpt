--TEST--
Partial application variation GC
--FILE--
<?php
#[AllowDynamicProperties]
class Foo {

  public function __construct($a) {
    $this->method = self::__construct(new stdClass, ...);
  }
}

$foo = new Foo(new stdClass);
$foo->bar = $foo;

echo "OK";
?>
--EXPECT--
OK
