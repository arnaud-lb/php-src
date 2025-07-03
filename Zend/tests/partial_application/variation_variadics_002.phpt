--TEST--
Partial application variation variadics internal
--FILE--
<?php
$sprintf = sprintf("%d %d %d", 100, ...);

echo (string) new ReflectionFunction($sprintf);

echo $sprintf(1000, 10000);
?>
--EXPECTF--
Partial [ <user> function sprintf ] {
  @@ %svariation_variadics_002.php 2 - 2

  - Parameters [1] {
    Parameter #0 [ <optional> mixed ...$values ]
  }
  - Return [ string ]
}
100 1000 10000
