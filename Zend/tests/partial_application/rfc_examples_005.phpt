--TEST--
Closure application RFC examples: errors
--FILE--
<?php

if (time() > 0) {
    function stuff(int $i, string $s, float $f, Point $p, int $m = 0) {}
}

stuff(?, ?, ?, p: $point, ?);

?>
--EXPECTF--
Fatal error: Cannot use positional argument after named argument in %s on line %d
