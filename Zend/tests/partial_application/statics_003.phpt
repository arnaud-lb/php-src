--TEST--
Closure application static variables are shared
--FILE--
<?php
$closure = function ($a) {
    static $var = 0;

    ++$var;

    return $var;
};

$closure(new stdClass);

$foo = $closure(?);
$closure = null;

$bar = $foo(?);
$foo = null;

if ($bar(new stdClass) == 2) {
    echo "OK";
}
?>
--EXPECT--
OK
