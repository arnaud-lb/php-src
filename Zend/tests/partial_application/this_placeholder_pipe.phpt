--TEST--
PFA with $this placeholder: pipe
--FILE--
<?php

echo new DateTimeImmutable('2026-01-19T19:44:42+00:00')
    |> DateTimeImmutable::format($this: ?, "c");

?>
--EXPECT--
2026-01-19T19:44:42+00:00
