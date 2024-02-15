--TEST--
Variance
--FILE--
<?php

class A {}
class B extends A {}

class Invariant<T> {}
class Out<out T> {}
class In<in T> {}

$objects = [
    'Invariant<A>' => new Invariant<A>,
    'Invariant<B>' => new Invariant<B>,
    'Out<A>' => new Out<A>,
    'Out<B>' => new Out<B>,
    'In<A>' => new In<A>,
    'In<B>' => new In<B>,
];

$functions = [
    Invariant::class => [
        function (Invariant<A> $a) {},
        function (Invariant<B> $b) {},
    ],
    Out::class => [
        function (Out<A> $a) {},
        function (Out<B> $b) {},
    ],
    In::class => [
        function (In<A> $a) {},
        function (In<B> $b) {},
    ],
];

foreach ($objects as $objectName => $object) {
    printf("%s:\n", $objectName);
    foreach ($functions[$object::class] as $function) {
        $rf = new ReflectionFunction($function);
        printf(" - function(%s): ", $rf->getParameters()[0]->getType()->__toString());
        try {
            $rf->invoke($object);
            printf("accepted\n");
        } catch (Error $e) {
            printf("%s\n", $e->getMessage());
        }
    }
}

?>
--EXPECT--
Invariant<A>:
 - function(Invariant<A>): accepted
 - function(Invariant<B>): {closure}(): Argument #1 ($b) must be of type Invariant<B>, Invariant given
Invariant<B>:
 - function(Invariant<A>): {closure}(): Argument #1 ($a) must be of type Invariant<A>, Invariant given
 - function(Invariant<B>): accepted
Out<A>:
 - function(Out<A>): accepted
 - function(Out<B>): {closure}(): Argument #1 ($b) must be of type Out<B>, Out given
Out<B>:
 - function(Out<A>): accepted
 - function(Out<B>): accepted
In<A>:
 - function(In<A>): accepted
 - function(In<B>): accepted
In<B>:
 - function(In<A>): {closure}(): Argument #1 ($a) must be of type In<A>, In given
 - function(In<B>): accepted
