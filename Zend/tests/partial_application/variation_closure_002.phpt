--TEST--
Partial application variation closure __invoke
--FILE--
<?php
$closure = function($a, $b) {
    var_dump($a, $b);
};

$function = $closure->__invoke(1, ?);

echo (string) new ReflectionFunction($function);

$function(10);
?>
--EXPECTF--
Partial [ <user, prototype Closure> public method __invoke ] {
  @@ %svariation_closure_002.php 6 - 6

  - Parameters [1] {
    Parameter #0 [ <required> $b ]
  }
}
int(1)
int(10)
