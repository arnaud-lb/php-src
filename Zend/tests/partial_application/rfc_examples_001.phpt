--TEST--
Closure application RFC examples: equivalence
--FILE--
<?php

function stuff(int $i, string $s, float $f, Point $p, int $m = 0): array {
    return [$i, $s, $f, $p, $m];
}

class Point {
}

$point = new Point();

$tests = [
    'Manually specify the first two values, and pull the rest "as is"' => [
        stuff(?, ?, ?, ?, ?),
        fn(int $i, string $s, float $f, Point $p, int $m = 0): array => stuff($i, $s, $f, $p, $m),
    ],
    'Manually specify the first two values, and pull the rest "as is" (2)' => [
        stuff(?, ?, ...),
        fn(int $i, string $s, float $f, Point $p, int $m = 0): array => stuff($i, $s, $f, $p, $m),
    ],
    'The degenerate "first class callables" case. (Supported since 8.1)' => [
        stuff(...),
        fn(int $i, string $s, float $f, Point $p, int $m = 0): array => stuff($i, $s, $f, $p, $m),
    ],
    'Provide some values, require the rest to be provided later' => [
        stuff(1, 'hi', ?, ?, ?),
        fn(float $f, Point $p, int $m = 0): array => stuff(1, 'hi', $f, $p, $m),
    ],
    'Provide some values, require the rest to be provided later (2)' => [
        stuff(1, 'hi', ...),
        fn(float $f, Point $p, int $m = 0): array => stuff(1, 'hi', $f, $p, $m),
    ],
    'Provided some values, but not just from the left' => [
        stuff(1, ?, 3.5, ?, ?),
        fn(string $s, Point $p, int $m = 0): array => stuff(1, $s, 3.5, $p, $m),
    ],
    'Provided some values, but not just from the left (2)' => [
        stuff(1, ?, 3.5, ...),
        fn(string $s, Point $p, int $m = 0): array => stuff(1, $s, 3.5, $p, $m),
    ],
    'Provide just the last value' => [
        stuff(?, ?, ?, ?, 5),
        fn(int $i, string $s, float $f, Point $p): array => stuff($i, $s, $f, $p, 5),
    ],
    'Not accounting for an optional argument means it will always get its default value' => [
        stuff(?, ?, ?, ?),
        fn(int $i, string $s, float $f, Point $p): array => stuff($i, $s, $f, $p),
    ],
    'Named arguments can be pulled "out of order", and still work' => [
        stuff(?, ?, f: 3.5, p: $point),
        fn(int $i, string $s): array => stuff($i, $s, 3.5, $point),
    ],
    'Named arguments can be pulled "out of order", and still work (2)' => [
        stuff(?, ?, p: $point, f: 3.5),
        fn(int $i, string $s): array => stuff($i, $s, 3.5, $point),
    ],
    'The ... "everything else" placeholder respects named arguments' => [
        stuff(?, ?, ..., f: 3.5, p: $point),
        fn(int $i, string $s, int $m = 0): array => stuff($i, $s, 3.5, $point, $m),
    ],
    'Prefill all parameters, making a "delayed call" or "thunk"' => [
        stuff(1, 'hi', 3.4, $point, 5, ...),
        fn(): array => stuff(1, 'hi', 3.4, $point, 5),
    ],
    'Placeholders may be named, too.  Their order doesn\'t matter as long as they come after the ..., if any' => [
        stuff(?, p: $point, f: ?, s: ?, m: 4),
        fn(int $i, string $s, float $f): array => stuff($i, $s, $f, $point, 4),
    ],
    'Placeholders may be named, too.  Their order doesn\'t matter as long as they come after the ..., if any (2)' => [
        stuff(..., m: 4, p: $point, i: ?),
        fn(int $i, string $s, float $f): array => stuff($i, $s, $f, $point, 4),
    ],
];

foreach ($tests as $test => [$pfa, $closure]) {
    echo "# ", $test, "\n";
    $pfaReflector = new ReflectionFunction($pfa);
    $closureReflector = new ReflectionFunction($closure);

    try {
        if (count($pfaReflector->getParameters()) !== count($closureReflector->getParameters())) {
            throw new Exception("Arity does not match");
        }

        $it = new MultipleIterator();
        $it->attachIterator(new ArrayIterator($pfaReflector->getParameters()));
        $it->attachIterator(new ArrayIterator($closureReflector->getParameters()));
        foreach ($it as $i => [$pfaParam, $closureParam]) {
            [$i] = $i;
            if ($pfaParam->getName() !== $closureParam->getName()) {
                throw new Exception(sprintf("Name of param %d does not match: %s vs %s",
                    $i,
                    $pfaParam->getName(),
                    $closureParam->getName(),
                ));
            }
            if ((string)$pfaParam->getType() !== (string)$closureParam->getType()) {
                throw new Exception(sprintf("Type of param %d does not match: %s vs %s",
                    $i,
                    $pfaParam->getType(),
                    $closureParam->getType(),
                ));
            }
            if ($pfaParam->isOptional() !== $closureParam->isOptional()) {
                throw new Exception(sprintf("Optionalness of param %d does not match: %d vs %d",
                    $i,
                    $pfaParam->isOptional(),
                    $closureParam->isOptional(),
                ));
            }
        }
    } catch (Exception $e) {
        echo $e->getMessage(), "\n";
        echo $pfaReflector;
        echo $closureReflector;
    }

    $args = [];
    foreach ($pfaReflector->getParameters() as $i => $p) {
        $args[] = match ((string) $p->getType()) {
            'int' => 100 + $i,
            'float' => 100.5 + $i,
            'string' => (string) (100 + $i),
            'Point' => new Point,
        };
    }

    if ($pfa(...$args) !== $closure(...$args)) {
        echo "PFA is not equivalent to closure\n";
    }
}

?>
--EXPECT--
# Manually specify the first two values, and pull the rest "as is"
# Manually specify the first two values, and pull the rest "as is" (2)
# The degenerate "first class callables" case. (Supported since 8.1)
# Provide some values, require the rest to be provided later
# Provide some values, require the rest to be provided later (2)
# Provided some values, but not just from the left
# Provided some values, but not just from the left (2)
# Provide just the last value
# Not accounting for an optional argument means it will always get its default value
# Named arguments can be pulled "out of order", and still work
# Named arguments can be pulled "out of order", and still work (2)
# The ... "everything else" placeholder respects named arguments
# Prefill all parameters, making a "delayed call" or "thunk"
# Placeholders may be named, too.  Their order doesn't matter as long as they come after the ..., if any
# Placeholders may be named, too.  Their order doesn't matter as long as they come after the ..., if any (2)
