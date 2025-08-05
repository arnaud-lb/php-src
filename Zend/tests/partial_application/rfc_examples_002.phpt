--TEST--
Closure application RFC examples: variadics equivalence
--XFAIL--
Name of params for positional placeholders that run into the variadic portion is wrong
--FILE--
<?php

function things(int $i, ?float $f = null, Point ...$points): array {
    return [$i, $f, $points];
}

class Point {
}

$point = new Point();

$tests = [
    'FCC equivalent. The signature is unchanged' => [
        things(...),
        fn(int $i, ?float $f = null, Point ...$points): array => things(...[$i, $f, ...$points]),
    ],
    'Provide some values, but allow the variadic to remain variadic' => [
        things(1, 3.14, ...),
        fn(Point ...$points): array => things(...[1, 3.14, ...$points]),
    ],
    'In this version, the partial requires precisely four arguments, the last two of which will get received by things() in the variadic parameter. Note too that $f becomes required in this case.' => [
        things(?, ?, ?, ?),
        fn(int $i, ?float $f, Point $p1, Point $p2): array => things($i, $f, $p1, $p2),
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
            '?float' => 100.5 + $i,
            'string' => (string) (100 + $i),
            'Point' => new Point,
        };
    }

    if ($pfa(...$args) !== $closure(...$args)) {
        echo "PFA is not equivalent to closure\n";
    }
}

?>
--EXPECTF--
# FCC equivalent. The signature is unchanged
# Provide some values, but allow the variadic to remain variadic
# In this version, the partial requires precisely four arguments, the last two of which will get received by things() in the variadic parameter. Note too that $f becomes required in this case.
Name of param 2 does not match: points vs p1
Closure [ <user> function things ] {
  @@ %srfc_examples_002.php 22 - 22

  - Parameters [4] {
    Parameter #0 [ <required> int $i ]
    Parameter #1 [ <required> ?float $f ]
    Parameter #2 [ <required> Point $points ]
    Parameter #3 [ <required> Point $points ]
  }
  - Return [ array ]
}
Closure [ <user> function {closure:%s:%d} ] {
  @@ %srfc_examples_002.php 23 - 23

  - Parameters [4] {
    Parameter #0 [ <required> int $i ]
    Parameter #1 [ <required> ?float $f ]
    Parameter #2 [ <required> Point $p1 ]
    Parameter #3 [ <required> Point $p2 ]
  }
  - Return [ array ]
}
