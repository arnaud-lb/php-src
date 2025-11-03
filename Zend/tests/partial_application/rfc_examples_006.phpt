--TEST--
Closure application RFC examples: func_get_args()
--FILE--
<?php

function f($a = 0, $b = 0, $c = 3, $d = 4) {
    echo func_num_args() . PHP_EOL;

    var_dump($a, $b, $c, $d);
}

f(1, 2);

$f = f(?, ?);

$f(1, 2);

?>
--EXPECT--
2
int(1)
int(2)
int(3)
int(4)
2
int(1)
int(2)
int(3)
int(4)
