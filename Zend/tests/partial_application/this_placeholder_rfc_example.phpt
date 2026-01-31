--TEST--
PFA with $this placeholder: RFC example
--FILE--
<?php

$dates = [
    new DateTimeImmutable('2026-01-19T19:44:42+00:00'),
];

$formattedDates = array_map(DateTimeImmutable::format(this: ?, "c"), $dates);

echo implode(", ", $formattedDates), PHP_EOL;

?>
--EXPECT--
2026-01-19T19:44:42+00:00
