--TEST--
Type checking
--FILE--
<?php

class C<out T> {}
class Base {}
interface I {}
class D extends Base implements I {}

function f1(C<int> $a) {}
function f2(C<string> $a) {}
function f3(C<int|string> $a) {}
function f4(C<stdClass> $a) {}
function f5(C<int|stdClass> $a) {}
function f6(C<int|stdClass|DateTime> $a) {}
function f7(C<I> $a) {}
function f8(C<Base&I> $a) {}
function f9(C<(Base&I)|stdClass> $a) {}
function f10(C<C<int> > $a) {}
function f11(C<C<stdClass>|int> $a) {}
function f12(array $a) {}
function f13(C<array> $a) {}
function f14(C<object> $a) {}
function f15(C<int>|stdClass $a) {}
function f16(C<int>|null $a) {}

class Test<T,U> {
    function m1(C<T> $a) {}
    function m2(C<T|U> $a) {}
    function m3(C<T|stdClass> $a) {}
    function m4(C<T|int> $a) {}
    function m5(C<T|null> $a) {}
    function m6(C<T&I> $a) {}
}

function test(ReflectionFunctionAbstract $rf, object $object = null, string $objectName = null) {
    $functionName = $rf->getName();
    if ($object) {
        $functionName = sprintf('%s::%s', $objectName, $functionName);
    }
    $typeName = $rf->getParameters()[0]->getType()->__toString();

    printf("function %s(%s):\n", $functionName, $typeName);

    $values = [
        'mixed' => new C<mixed>(),
        'int' => new C<int>(),
        'string' => new C<string>(),
        'string|int' => new C<string|int>(),
        'stdClass' => new C<stdClass>(),
        'stdClass|int' => new C<stdClass|int>(),
        'stdClass|DateTime|int' => new C<stdClass|DateTime|int>(),
        'Base' => new C<Base>(),
        'D' => new C<D>(),
        'C<int>' => new C<C<int> >(),
        'C<stdClass>|int' => new C<C<stdClass>|int>(),
        'array' => new C<array>(),
        'object' => new C<object>(),
    ];

    foreach ($values as $label => $value) {
        try {
            if ($object) {
                $rf->invoke($object, $value);
            } else {
                $rf->invoke($value);
            }
            printf(" - C<%s>: accepted\n", $label);
        } catch (Error $e) {
            printf(" - C<%s>: %s\n", $label, $e->getMessage());
        }
    }
}

$functions = get_defined_functions()['user'];

foreach ($functions as $function) {
    $rf = new ReflectionFunction($function);
    if ($rf->getFilename() !== __FILE__ || !str_starts_with($function, 'f')) {
        continue;
    }

    test($rf);
}

$methods = (new ReflectionClass(Test::class))->getMethods();

$objects = [
    'Test<stdClass,stdClass>' => new Test<stdClass,stdClass>(),
    'Test<stdClass,int>' => new Test<stdClass,int>(),
    'Test<int,int>' => new Test<int,int>(),
    'Test<object,object>' => new Test<object,object>(),
    'Test<object,stdClass>' => new Test<object,stdClass>(),
    'Test<int|float,int|float>' => new Test<int|float,int|float>(),
    'Test<stdClass|DateTime,stdClass|DateTime>' => new Test<stdClass|DateTime,stdClass|DateTime>(),
    'Test<stdClass|DateTime|int,stdClass|DateTime|int>' => new Test<stdClass|DateTime|int,stdClass|DateTime|int>(),
    'Test<Base&I,Base&I>' => new Test<Base&I,Base&I>(),
    'Test<Base|stdClass,int>' => new Test<Base|stdClass,int>(),
];

foreach ($objects as $objectName => $object) {
    foreach ($methods as $method) {
        test($method, $object, $objectName);
    }
}

?>
--EXPECT--
function f1(C<int>):
 - C<mixed>: f1(): Argument #1 ($a) must be of type C<int>, C given
 - C<int>: accepted
 - C<string>: f1(): Argument #1 ($a) must be of type C<int>, C given
 - C<string|int>: f1(): Argument #1 ($a) must be of type C<int>, C given
 - C<stdClass>: f1(): Argument #1 ($a) must be of type C<int>, C given
 - C<stdClass|int>: f1(): Argument #1 ($a) must be of type C<int>, C given
 - C<stdClass|DateTime|int>: f1(): Argument #1 ($a) must be of type C<int>, C given
 - C<Base>: f1(): Argument #1 ($a) must be of type C<int>, C given
 - C<D>: f1(): Argument #1 ($a) must be of type C<int>, C given
 - C<C<int>>: f1(): Argument #1 ($a) must be of type C<int>, C given
 - C<C<stdClass>|int>: f1(): Argument #1 ($a) must be of type C<int>, C given
 - C<array>: f1(): Argument #1 ($a) must be of type C<int>, C given
 - C<object>: f1(): Argument #1 ($a) must be of type C<int>, C given
function f2(C<string>):
 - C<mixed>: f2(): Argument #1 ($a) must be of type C<string>, C given
 - C<int>: f2(): Argument #1 ($a) must be of type C<string>, C given
 - C<string>: accepted
 - C<string|int>: f2(): Argument #1 ($a) must be of type C<string>, C given
 - C<stdClass>: f2(): Argument #1 ($a) must be of type C<string>, C given
 - C<stdClass|int>: f2(): Argument #1 ($a) must be of type C<string>, C given
 - C<stdClass|DateTime|int>: f2(): Argument #1 ($a) must be of type C<string>, C given
 - C<Base>: f2(): Argument #1 ($a) must be of type C<string>, C given
 - C<D>: f2(): Argument #1 ($a) must be of type C<string>, C given
 - C<C<int>>: f2(): Argument #1 ($a) must be of type C<string>, C given
 - C<C<stdClass>|int>: f2(): Argument #1 ($a) must be of type C<string>, C given
 - C<array>: f2(): Argument #1 ($a) must be of type C<string>, C given
 - C<object>: f2(): Argument #1 ($a) must be of type C<string>, C given
function f3(C<string|int>):
 - C<mixed>: f3(): Argument #1 ($a) must be of type C<string|int>, C given
 - C<int>: accepted
 - C<string>: accepted
 - C<string|int>: accepted
 - C<stdClass>: f3(): Argument #1 ($a) must be of type C<string|int>, C given
 - C<stdClass|int>: f3(): Argument #1 ($a) must be of type C<string|int>, C given
 - C<stdClass|DateTime|int>: f3(): Argument #1 ($a) must be of type C<string|int>, C given
 - C<Base>: f3(): Argument #1 ($a) must be of type C<string|int>, C given
 - C<D>: f3(): Argument #1 ($a) must be of type C<string|int>, C given
 - C<C<int>>: f3(): Argument #1 ($a) must be of type C<string|int>, C given
 - C<C<stdClass>|int>: f3(): Argument #1 ($a) must be of type C<string|int>, C given
 - C<array>: f3(): Argument #1 ($a) must be of type C<string|int>, C given
 - C<object>: f3(): Argument #1 ($a) must be of type C<string|int>, C given
function f4(C<stdClass>):
 - C<mixed>: f4(): Argument #1 ($a) must be of type C<stdClass>, C given
 - C<int>: f4(): Argument #1 ($a) must be of type C<stdClass>, C given
 - C<string>: f4(): Argument #1 ($a) must be of type C<stdClass>, C given
 - C<string|int>: f4(): Argument #1 ($a) must be of type C<stdClass>, C given
 - C<stdClass>: accepted
 - C<stdClass|int>: f4(): Argument #1 ($a) must be of type C<stdClass>, C given
 - C<stdClass|DateTime|int>: f4(): Argument #1 ($a) must be of type C<stdClass>, C given
 - C<Base>: f4(): Argument #1 ($a) must be of type C<stdClass>, C given
 - C<D>: f4(): Argument #1 ($a) must be of type C<stdClass>, C given
 - C<C<int>>: f4(): Argument #1 ($a) must be of type C<stdClass>, C given
 - C<C<stdClass>|int>: f4(): Argument #1 ($a) must be of type C<stdClass>, C given
 - C<array>: f4(): Argument #1 ($a) must be of type C<stdClass>, C given
 - C<object>: f4(): Argument #1 ($a) must be of type C<stdClass>, C given
function f5(C<stdClass|int>):
 - C<mixed>: f5(): Argument #1 ($a) must be of type C<stdClass|int>, C given
 - C<int>: accepted
 - C<string>: f5(): Argument #1 ($a) must be of type C<stdClass|int>, C given
 - C<string|int>: f5(): Argument #1 ($a) must be of type C<stdClass|int>, C given
 - C<stdClass>: accepted
 - C<stdClass|int>: accepted
 - C<stdClass|DateTime|int>: f5(): Argument #1 ($a) must be of type C<stdClass|int>, C given
 - C<Base>: f5(): Argument #1 ($a) must be of type C<stdClass|int>, C given
 - C<D>: f5(): Argument #1 ($a) must be of type C<stdClass|int>, C given
 - C<C<int>>: f5(): Argument #1 ($a) must be of type C<stdClass|int>, C given
 - C<C<stdClass>|int>: f5(): Argument #1 ($a) must be of type C<stdClass|int>, C given
 - C<array>: f5(): Argument #1 ($a) must be of type C<stdClass|int>, C given
 - C<object>: f5(): Argument #1 ($a) must be of type C<stdClass|int>, C given
function f6(C<stdClass|DateTime|int>):
 - C<mixed>: f6(): Argument #1 ($a) must be of type C<stdClass|DateTime|int>, C given
 - C<int>: accepted
 - C<string>: f6(): Argument #1 ($a) must be of type C<stdClass|DateTime|int>, C given
 - C<string|int>: f6(): Argument #1 ($a) must be of type C<stdClass|DateTime|int>, C given
 - C<stdClass>: accepted
 - C<stdClass|int>: accepted
 - C<stdClass|DateTime|int>: accepted
 - C<Base>: f6(): Argument #1 ($a) must be of type C<stdClass|DateTime|int>, C given
 - C<D>: f6(): Argument #1 ($a) must be of type C<stdClass|DateTime|int>, C given
 - C<C<int>>: f6(): Argument #1 ($a) must be of type C<stdClass|DateTime|int>, C given
 - C<C<stdClass>|int>: f6(): Argument #1 ($a) must be of type C<stdClass|DateTime|int>, C given
 - C<array>: f6(): Argument #1 ($a) must be of type C<stdClass|DateTime|int>, C given
 - C<object>: f6(): Argument #1 ($a) must be of type C<stdClass|DateTime|int>, C given
function f7(C<I>):
 - C<mixed>: f7(): Argument #1 ($a) must be of type C<I>, C given
 - C<int>: f7(): Argument #1 ($a) must be of type C<I>, C given
 - C<string>: f7(): Argument #1 ($a) must be of type C<I>, C given
 - C<string|int>: f7(): Argument #1 ($a) must be of type C<I>, C given
 - C<stdClass>: f7(): Argument #1 ($a) must be of type C<I>, C given
 - C<stdClass|int>: f7(): Argument #1 ($a) must be of type C<I>, C given
 - C<stdClass|DateTime|int>: f7(): Argument #1 ($a) must be of type C<I>, C given
 - C<Base>: f7(): Argument #1 ($a) must be of type C<I>, C given
 - C<D>: accepted
 - C<C<int>>: f7(): Argument #1 ($a) must be of type C<I>, C given
 - C<C<stdClass>|int>: f7(): Argument #1 ($a) must be of type C<I>, C given
 - C<array>: f7(): Argument #1 ($a) must be of type C<I>, C given
 - C<object>: f7(): Argument #1 ($a) must be of type C<I>, C given
function f8(C<Base&I>):
 - C<mixed>: f8(): Argument #1 ($a) must be of type C<Base&I>, C given
 - C<int>: f8(): Argument #1 ($a) must be of type C<Base&I>, C given
 - C<string>: f8(): Argument #1 ($a) must be of type C<Base&I>, C given
 - C<string|int>: f8(): Argument #1 ($a) must be of type C<Base&I>, C given
 - C<stdClass>: f8(): Argument #1 ($a) must be of type C<Base&I>, C given
 - C<stdClass|int>: f8(): Argument #1 ($a) must be of type C<Base&I>, C given
 - C<stdClass|DateTime|int>: f8(): Argument #1 ($a) must be of type C<Base&I>, C given
 - C<Base>: f8(): Argument #1 ($a) must be of type C<Base&I>, C given
 - C<D>: accepted
 - C<C<int>>: f8(): Argument #1 ($a) must be of type C<Base&I>, C given
 - C<C<stdClass>|int>: f8(): Argument #1 ($a) must be of type C<Base&I>, C given
 - C<array>: f8(): Argument #1 ($a) must be of type C<Base&I>, C given
 - C<object>: f8(): Argument #1 ($a) must be of type C<Base&I>, C given
function f9(C<(Base&I)|stdClass>):
 - C<mixed>: f9(): Argument #1 ($a) must be of type C<(Base&I)|stdClass>, C given
 - C<int>: f9(): Argument #1 ($a) must be of type C<(Base&I)|stdClass>, C given
 - C<string>: f9(): Argument #1 ($a) must be of type C<(Base&I)|stdClass>, C given
 - C<string|int>: f9(): Argument #1 ($a) must be of type C<(Base&I)|stdClass>, C given
 - C<stdClass>: accepted
 - C<stdClass|int>: f9(): Argument #1 ($a) must be of type C<(Base&I)|stdClass>, C given
 - C<stdClass|DateTime|int>: f9(): Argument #1 ($a) must be of type C<(Base&I)|stdClass>, C given
 - C<Base>: f9(): Argument #1 ($a) must be of type C<(Base&I)|stdClass>, C given
 - C<D>: accepted
 - C<C<int>>: f9(): Argument #1 ($a) must be of type C<(Base&I)|stdClass>, C given
 - C<C<stdClass>|int>: f9(): Argument #1 ($a) must be of type C<(Base&I)|stdClass>, C given
 - C<array>: f9(): Argument #1 ($a) must be of type C<(Base&I)|stdClass>, C given
 - C<object>: f9(): Argument #1 ($a) must be of type C<(Base&I)|stdClass>, C given
function f10(C<C<int>>):
 - C<mixed>: f10(): Argument #1 ($a) must be of type C<C<int>>, C given
 - C<int>: f10(): Argument #1 ($a) must be of type C<C<int>>, C given
 - C<string>: f10(): Argument #1 ($a) must be of type C<C<int>>, C given
 - C<string|int>: f10(): Argument #1 ($a) must be of type C<C<int>>, C given
 - C<stdClass>: f10(): Argument #1 ($a) must be of type C<C<int>>, C given
 - C<stdClass|int>: f10(): Argument #1 ($a) must be of type C<C<int>>, C given
 - C<stdClass|DateTime|int>: f10(): Argument #1 ($a) must be of type C<C<int>>, C given
 - C<Base>: f10(): Argument #1 ($a) must be of type C<C<int>>, C given
 - C<D>: f10(): Argument #1 ($a) must be of type C<C<int>>, C given
 - C<C<int>>: accepted
 - C<C<stdClass>|int>: f10(): Argument #1 ($a) must be of type C<C<int>>, C given
 - C<array>: f10(): Argument #1 ($a) must be of type C<C<int>>, C given
 - C<object>: f10(): Argument #1 ($a) must be of type C<C<int>>, C given
function f11(C<C<stdClass>|int>):
 - C<mixed>: f11(): Argument #1 ($a) must be of type C<C<stdClass>|int>, C given
 - C<int>: accepted
 - C<string>: f11(): Argument #1 ($a) must be of type C<C<stdClass>|int>, C given
 - C<string|int>: f11(): Argument #1 ($a) must be of type C<C<stdClass>|int>, C given
 - C<stdClass>: f11(): Argument #1 ($a) must be of type C<C<stdClass>|int>, C given
 - C<stdClass|int>: f11(): Argument #1 ($a) must be of type C<C<stdClass>|int>, C given
 - C<stdClass|DateTime|int>: f11(): Argument #1 ($a) must be of type C<C<stdClass>|int>, C given
 - C<Base>: f11(): Argument #1 ($a) must be of type C<C<stdClass>|int>, C given
 - C<D>: f11(): Argument #1 ($a) must be of type C<C<stdClass>|int>, C given
 - C<C<int>>: f11(): Argument #1 ($a) must be of type C<C<stdClass>|int>, C given
 - C<C<stdClass>|int>: accepted
 - C<array>: f11(): Argument #1 ($a) must be of type C<C<stdClass>|int>, C given
 - C<object>: f11(): Argument #1 ($a) must be of type C<C<stdClass>|int>, C given
function f12(array):
 - C<mixed>: f12(): Argument #1 ($a) must be of type array, C given
 - C<int>: f12(): Argument #1 ($a) must be of type array, C given
 - C<string>: f12(): Argument #1 ($a) must be of type array, C given
 - C<string|int>: f12(): Argument #1 ($a) must be of type array, C given
 - C<stdClass>: f12(): Argument #1 ($a) must be of type array, C given
 - C<stdClass|int>: f12(): Argument #1 ($a) must be of type array, C given
 - C<stdClass|DateTime|int>: f12(): Argument #1 ($a) must be of type array, C given
 - C<Base>: f12(): Argument #1 ($a) must be of type array, C given
 - C<D>: f12(): Argument #1 ($a) must be of type array, C given
 - C<C<int>>: f12(): Argument #1 ($a) must be of type array, C given
 - C<C<stdClass>|int>: f12(): Argument #1 ($a) must be of type array, C given
 - C<array>: f12(): Argument #1 ($a) must be of type array, C given
 - C<object>: f12(): Argument #1 ($a) must be of type array, C given
function f13(C<array>):
 - C<mixed>: f13(): Argument #1 ($a) must be of type C<array>, C given
 - C<int>: f13(): Argument #1 ($a) must be of type C<array>, C given
 - C<string>: f13(): Argument #1 ($a) must be of type C<array>, C given
 - C<string|int>: f13(): Argument #1 ($a) must be of type C<array>, C given
 - C<stdClass>: f13(): Argument #1 ($a) must be of type C<array>, C given
 - C<stdClass|int>: f13(): Argument #1 ($a) must be of type C<array>, C given
 - C<stdClass|DateTime|int>: f13(): Argument #1 ($a) must be of type C<array>, C given
 - C<Base>: f13(): Argument #1 ($a) must be of type C<array>, C given
 - C<D>: f13(): Argument #1 ($a) must be of type C<array>, C given
 - C<C<int>>: f13(): Argument #1 ($a) must be of type C<array>, C given
 - C<C<stdClass>|int>: f13(): Argument #1 ($a) must be of type C<array>, C given
 - C<array>: accepted
 - C<object>: f13(): Argument #1 ($a) must be of type C<array>, C given
function f14(C<object>):
 - C<mixed>: f14(): Argument #1 ($a) must be of type C<object>, C given
 - C<int>: f14(): Argument #1 ($a) must be of type C<object>, C given
 - C<string>: f14(): Argument #1 ($a) must be of type C<object>, C given
 - C<string|int>: f14(): Argument #1 ($a) must be of type C<object>, C given
 - C<stdClass>: accepted
 - C<stdClass|int>: f14(): Argument #1 ($a) must be of type C<object>, C given
 - C<stdClass|DateTime|int>: accepted
 - C<Base>: accepted
 - C<D>: accepted
 - C<C<int>>: accepted
 - C<C<stdClass>|int>: f14(): Argument #1 ($a) must be of type C<object>, C given
 - C<array>: f14(): Argument #1 ($a) must be of type C<object>, C given
 - C<object>: accepted
function f15(C<int>|stdClass):
 - C<mixed>: f15(): Argument #1 ($a) must be of type C<int>|stdClass, C given
 - C<int>: accepted
 - C<string>: f15(): Argument #1 ($a) must be of type C<int>|stdClass, C given
 - C<string|int>: f15(): Argument #1 ($a) must be of type C<int>|stdClass, C given
 - C<stdClass>: f15(): Argument #1 ($a) must be of type C<int>|stdClass, C given
 - C<stdClass|int>: f15(): Argument #1 ($a) must be of type C<int>|stdClass, C given
 - C<stdClass|DateTime|int>: f15(): Argument #1 ($a) must be of type C<int>|stdClass, C given
 - C<Base>: f15(): Argument #1 ($a) must be of type C<int>|stdClass, C given
 - C<D>: f15(): Argument #1 ($a) must be of type C<int>|stdClass, C given
 - C<C<int>>: f15(): Argument #1 ($a) must be of type C<int>|stdClass, C given
 - C<C<stdClass>|int>: f15(): Argument #1 ($a) must be of type C<int>|stdClass, C given
 - C<array>: f15(): Argument #1 ($a) must be of type C<int>|stdClass, C given
 - C<object>: f15(): Argument #1 ($a) must be of type C<int>|stdClass, C given
function f16(?C<int>):
 - C<mixed>: f16(): Argument #1 ($a) must be of type ?C<int>, C given
 - C<int>: accepted
 - C<string>: f16(): Argument #1 ($a) must be of type ?C<int>, C given
 - C<string|int>: f16(): Argument #1 ($a) must be of type ?C<int>, C given
 - C<stdClass>: f16(): Argument #1 ($a) must be of type ?C<int>, C given
 - C<stdClass|int>: f16(): Argument #1 ($a) must be of type ?C<int>, C given
 - C<stdClass|DateTime|int>: f16(): Argument #1 ($a) must be of type ?C<int>, C given
 - C<Base>: f16(): Argument #1 ($a) must be of type ?C<int>, C given
 - C<D>: f16(): Argument #1 ($a) must be of type ?C<int>, C given
 - C<C<int>>: f16(): Argument #1 ($a) must be of type ?C<int>, C given
 - C<C<stdClass>|int>: f16(): Argument #1 ($a) must be of type ?C<int>, C given
 - C<array>: f16(): Argument #1 ($a) must be of type ?C<int>, C given
 - C<object>: f16(): Argument #1 ($a) must be of type ?C<int>, C given
function Test<stdClass,stdClass>::m1(C<0>):
 - C<mixed>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<string>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<string|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<stdClass|DateTime|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<Base>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<D>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<C<int>>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<C<stdClass>|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<array>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<object>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
function Test<stdClass,stdClass>::m2(C<0|1>):
 - C<mixed>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = stdClass), C given
 - C<int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = stdClass), C given
 - C<string>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = stdClass), C given
 - C<string|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = stdClass), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = stdClass), C given
 - C<stdClass|DateTime|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = stdClass), C given
 - C<Base>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = stdClass), C given
 - C<D>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = stdClass), C given
 - C<C<int>>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = stdClass), C given
 - C<C<stdClass>|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = stdClass), C given
 - C<array>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = stdClass), C given
 - C<object>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = stdClass), C given
function Test<stdClass,stdClass>::m3(C<0|stdClass>):
 - C<mixed>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<string>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<string|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<stdClass|DateTime|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<Base>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<D>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<C<int>>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<C<stdClass>|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<array>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<object>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
function Test<stdClass,stdClass>::m4(C<0|int>):
 - C<mixed>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<int>: accepted
 - C<string>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<string|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: accepted
 - C<stdClass|DateTime|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<Base>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<D>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<C<int>>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<C<stdClass>|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<array>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<object>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
function Test<stdClass,stdClass>::m5(C<?0>):
 - C<mixed>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<string>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<string|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<stdClass|DateTime|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<Base>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<D>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<C<int>>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<C<stdClass>|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<array>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<object>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
function Test<stdClass,stdClass>::m6(C<0&I>):
 - C<mixed>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<string>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<string|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<stdClass>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<stdClass|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<stdClass|DateTime|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<Base>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<D>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<C<int>>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<C<stdClass>|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<array>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<object>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
function Test<stdClass,int>::m1(C<0>):
 - C<mixed>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<string>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<string|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<stdClass|DateTime|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<Base>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<D>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<C<int>>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<C<stdClass>|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<array>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
 - C<object>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass), C given
function Test<stdClass,int>::m2(C<0|1>):
 - C<mixed>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = int), C given
 - C<int>: accepted
 - C<string>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = int), C given
 - C<string|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = int), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: accepted
 - C<stdClass|DateTime|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = int), C given
 - C<Base>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = int), C given
 - C<D>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = int), C given
 - C<C<int>>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = int), C given
 - C<C<stdClass>|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = int), C given
 - C<array>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = int), C given
 - C<object>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClassU = int), C given
function Test<stdClass,int>::m3(C<0|stdClass>):
 - C<mixed>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<string>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<string|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<stdClass|DateTime|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<Base>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<D>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<C<int>>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<C<stdClass>|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<array>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
 - C<object>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass), C given
function Test<stdClass,int>::m4(C<0|int>):
 - C<mixed>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<int>: accepted
 - C<string>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<string|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: accepted
 - C<stdClass|DateTime|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<Base>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<D>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<C<int>>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<C<stdClass>|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<array>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
 - C<object>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass), C given
function Test<stdClass,int>::m5(C<?0>):
 - C<mixed>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<string>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<string|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<stdClass|DateTime|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<Base>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<D>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<C<int>>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<C<stdClass>|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<array>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
 - C<object>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass), C given
function Test<stdClass,int>::m6(C<0&I>):
 - C<mixed>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<string>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<string|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<stdClass>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<stdClass|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<stdClass|DateTime|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<Base>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<D>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<C<int>>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<C<stdClass>|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<array>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
 - C<object>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass), C given
function Test<int,int>::m1(C<0>):
 - C<mixed>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int), C given
 - C<int>: accepted
 - C<string>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int), C given
 - C<string|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int), C given
 - C<stdClass>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int), C given
 - C<stdClass|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int), C given
 - C<stdClass|DateTime|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int), C given
 - C<Base>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int), C given
 - C<D>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int), C given
 - C<C<int>>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int), C given
 - C<C<stdClass>|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int), C given
 - C<array>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int), C given
 - C<object>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int), C given
function Test<int,int>::m2(C<0|1>):
 - C<mixed>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = intU = int), C given
 - C<int>: accepted
 - C<string>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = intU = int), C given
 - C<string|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = intU = int), C given
 - C<stdClass>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = intU = int), C given
 - C<stdClass|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = intU = int), C given
 - C<stdClass|DateTime|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = intU = int), C given
 - C<Base>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = intU = int), C given
 - C<D>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = intU = int), C given
 - C<C<int>>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = intU = int), C given
 - C<C<stdClass>|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = intU = int), C given
 - C<array>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = intU = int), C given
 - C<object>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = intU = int), C given
function Test<int,int>::m3(C<0|stdClass>):
 - C<mixed>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int), C given
 - C<int>: accepted
 - C<string>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int), C given
 - C<string|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: accepted
 - C<stdClass|DateTime|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int), C given
 - C<Base>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int), C given
 - C<D>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int), C given
 - C<C<int>>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int), C given
 - C<C<stdClass>|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int), C given
 - C<array>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int), C given
 - C<object>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int), C given
function Test<int,int>::m4(C<0|int>):
 - C<mixed>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int), C given
 - C<int>: accepted
 - C<string>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int), C given
 - C<string|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int), C given
 - C<stdClass>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int), C given
 - C<stdClass|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int), C given
 - C<stdClass|DateTime|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int), C given
 - C<Base>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int), C given
 - C<D>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int), C given
 - C<C<int>>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int), C given
 - C<C<stdClass>|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int), C given
 - C<array>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int), C given
 - C<object>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int), C given
function Test<int,int>::m5(C<?0>):
 - C<mixed>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int), C given
 - C<int>: accepted
 - C<string>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int), C given
 - C<string|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int), C given
 - C<stdClass>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int), C given
 - C<stdClass|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int), C given
 - C<stdClass|DateTime|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int), C given
 - C<Base>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int), C given
 - C<D>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int), C given
 - C<C<int>>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int), C given
 - C<C<stdClass>|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int), C given
 - C<array>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int), C given
 - C<object>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int), C given
function Test<int,int>::m6(C<0&I>):
 - C<mixed>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int), C given
 - C<int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int), C given
 - C<string>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int), C given
 - C<string|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int), C given
 - C<stdClass>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int), C given
 - C<stdClass|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int), C given
 - C<stdClass|DateTime|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int), C given
 - C<Base>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int), C given
 - C<D>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int), C given
 - C<C<int>>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int), C given
 - C<C<stdClass>|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int), C given
 - C<array>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int), C given
 - C<object>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int), C given
function Test<object,object>::m1(C<0>):
 - C<mixed>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = object), C given
 - C<int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = object), C given
 - C<string>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = object), C given
 - C<string|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = object), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = object), C given
 - C<stdClass|DateTime|int>: accepted
 - C<Base>: accepted
 - C<D>: accepted
 - C<C<int>>: accepted
 - C<C<stdClass>|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = object), C given
 - C<array>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = object), C given
 - C<object>: accepted
function Test<object,object>::m2(C<0|1>):
 - C<mixed>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = objectU = object), C given
 - C<int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = objectU = object), C given
 - C<string>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = objectU = object), C given
 - C<string|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = objectU = object), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = objectU = object), C given
 - C<stdClass|DateTime|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = objectU = object), C given
 - C<Base>: accepted
 - C<D>: accepted
 - C<C<int>>: accepted
 - C<C<stdClass>|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = objectU = object), C given
 - C<array>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = objectU = object), C given
 - C<object>: accepted
function Test<object,object>::m3(C<0|stdClass>):
 - C<mixed>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = object), C given
 - C<int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = object), C given
 - C<string>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = object), C given
 - C<string|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = object), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = object), C given
 - C<stdClass|DateTime|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = object), C given
 - C<Base>: accepted
 - C<D>: accepted
 - C<C<int>>: accepted
 - C<C<stdClass>|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = object), C given
 - C<array>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = object), C given
 - C<object>: accepted
function Test<object,object>::m4(C<0|int>):
 - C<mixed>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = object), C given
 - C<int>: accepted
 - C<string>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = object), C given
 - C<string|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = object), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: accepted
 - C<stdClass|DateTime|int>: accepted
 - C<Base>: accepted
 - C<D>: accepted
 - C<C<int>>: accepted
 - C<C<stdClass>|int>: accepted
 - C<array>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = object), C given
 - C<object>: accepted
function Test<object,object>::m5(C<?0>):
 - C<mixed>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = object), C given
 - C<int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = object), C given
 - C<string>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = object), C given
 - C<string|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = object), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = object), C given
 - C<stdClass|DateTime|int>: accepted
 - C<Base>: accepted
 - C<D>: accepted
 - C<C<int>>: accepted
 - C<C<stdClass>|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = object), C given
 - C<array>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = object), C given
 - C<object>: accepted
function Test<object,object>::m6(C<0&I>):
 - C<mixed>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<string>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<string|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<stdClass>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<stdClass|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<stdClass|DateTime|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<Base>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<D>: accepted
 - C<C<int>>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<C<stdClass>|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<array>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<object>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
function Test<object,stdClass>::m1(C<0>):
 - C<mixed>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = object), C given
 - C<int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = object), C given
 - C<string>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = object), C given
 - C<string|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = object), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = object), C given
 - C<stdClass|DateTime|int>: accepted
 - C<Base>: accepted
 - C<D>: accepted
 - C<C<int>>: accepted
 - C<C<stdClass>|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = object), C given
 - C<array>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = object), C given
 - C<object>: accepted
function Test<object,stdClass>::m2(C<0|1>):
 - C<mixed>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = objectU = stdClass), C given
 - C<int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = objectU = stdClass), C given
 - C<string>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = objectU = stdClass), C given
 - C<string|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = objectU = stdClass), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = objectU = stdClass), C given
 - C<stdClass|DateTime|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = objectU = stdClass), C given
 - C<Base>: accepted
 - C<D>: accepted
 - C<C<int>>: accepted
 - C<C<stdClass>|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = objectU = stdClass), C given
 - C<array>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = objectU = stdClass), C given
 - C<object>: accepted
function Test<object,stdClass>::m3(C<0|stdClass>):
 - C<mixed>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = object), C given
 - C<int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = object), C given
 - C<string>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = object), C given
 - C<string|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = object), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = object), C given
 - C<stdClass|DateTime|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = object), C given
 - C<Base>: accepted
 - C<D>: accepted
 - C<C<int>>: accepted
 - C<C<stdClass>|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = object), C given
 - C<array>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = object), C given
 - C<object>: accepted
function Test<object,stdClass>::m4(C<0|int>):
 - C<mixed>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = object), C given
 - C<int>: accepted
 - C<string>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = object), C given
 - C<string|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = object), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: accepted
 - C<stdClass|DateTime|int>: accepted
 - C<Base>: accepted
 - C<D>: accepted
 - C<C<int>>: accepted
 - C<C<stdClass>|int>: accepted
 - C<array>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = object), C given
 - C<object>: accepted
function Test<object,stdClass>::m5(C<?0>):
 - C<mixed>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = object), C given
 - C<int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = object), C given
 - C<string>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = object), C given
 - C<string|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = object), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = object), C given
 - C<stdClass|DateTime|int>: accepted
 - C<Base>: accepted
 - C<D>: accepted
 - C<C<int>>: accepted
 - C<C<stdClass>|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = object), C given
 - C<array>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = object), C given
 - C<object>: accepted
function Test<object,stdClass>::m6(C<0&I>):
 - C<mixed>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<string>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<string|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<stdClass>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<stdClass|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<stdClass|DateTime|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<Base>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<D>: accepted
 - C<C<int>>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<C<stdClass>|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<array>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
 - C<object>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = object), C given
function Test<int|float,int|float>::m1(C<0>):
 - C<mixed>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int|float), C given
 - C<int>: accepted
 - C<string>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int|float), C given
 - C<string|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int|float), C given
 - C<stdClass>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int|float), C given
 - C<stdClass|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int|float), C given
 - C<stdClass|DateTime|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int|float), C given
 - C<Base>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int|float), C given
 - C<D>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int|float), C given
 - C<C<int>>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int|float), C given
 - C<C<stdClass>|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int|float), C given
 - C<array>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int|float), C given
 - C<object>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = int|float), C given
function Test<int|float,int|float>::m2(C<0|1>):
 - C<mixed>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = int|floatU = int|float), C given
 - C<int>: accepted
 - C<string>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = int|floatU = int|float), C given
 - C<string|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = int|floatU = int|float), C given
 - C<stdClass>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = int|floatU = int|float), C given
 - C<stdClass|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = int|floatU = int|float), C given
 - C<stdClass|DateTime|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = int|floatU = int|float), C given
 - C<Base>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = int|floatU = int|float), C given
 - C<D>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = int|floatU = int|float), C given
 - C<C<int>>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = int|floatU = int|float), C given
 - C<C<stdClass>|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = int|floatU = int|float), C given
 - C<array>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = int|floatU = int|float), C given
 - C<object>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = int|floatU = int|float), C given
function Test<int|float,int|float>::m3(C<0|stdClass>):
 - C<mixed>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int|float), C given
 - C<int>: accepted
 - C<string>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int|float), C given
 - C<string|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int|float), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: accepted
 - C<stdClass|DateTime|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int|float), C given
 - C<Base>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int|float), C given
 - C<D>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int|float), C given
 - C<C<int>>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int|float), C given
 - C<C<stdClass>|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int|float), C given
 - C<array>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int|float), C given
 - C<object>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = int|float), C given
function Test<int|float,int|float>::m4(C<0|int>):
 - C<mixed>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int|float), C given
 - C<int>: accepted
 - C<string>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int|float), C given
 - C<string|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int|float), C given
 - C<stdClass>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int|float), C given
 - C<stdClass|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int|float), C given
 - C<stdClass|DateTime|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int|float), C given
 - C<Base>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int|float), C given
 - C<D>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int|float), C given
 - C<C<int>>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int|float), C given
 - C<C<stdClass>|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int|float), C given
 - C<array>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int|float), C given
 - C<object>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = int|float), C given
function Test<int|float,int|float>::m5(C<?0>):
 - C<mixed>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int|float), C given
 - C<int>: accepted
 - C<string>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int|float), C given
 - C<string|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int|float), C given
 - C<stdClass>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int|float), C given
 - C<stdClass|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int|float), C given
 - C<stdClass|DateTime|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int|float), C given
 - C<Base>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int|float), C given
 - C<D>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int|float), C given
 - C<C<int>>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int|float), C given
 - C<C<stdClass>|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int|float), C given
 - C<array>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int|float), C given
 - C<object>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = int|float), C given
function Test<int|float,int|float>::m6(C<0&I>):
 - C<mixed>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int|float), C given
 - C<int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int|float), C given
 - C<string>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int|float), C given
 - C<string|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int|float), C given
 - C<stdClass>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int|float), C given
 - C<stdClass|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int|float), C given
 - C<stdClass|DateTime|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int|float), C given
 - C<Base>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int|float), C given
 - C<D>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int|float), C given
 - C<C<int>>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int|float), C given
 - C<C<stdClass>|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int|float), C given
 - C<array>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int|float), C given
 - C<object>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = int|float), C given
function Test<stdClass|DateTime,stdClass|DateTime>::m1(C<0>):
 - C<mixed>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime), C given
 - C<int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime), C given
 - C<string>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime), C given
 - C<string|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime), C given
 - C<stdClass|DateTime|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime), C given
 - C<Base>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime), C given
 - C<D>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime), C given
 - C<C<int>>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime), C given
 - C<C<stdClass>|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime), C given
 - C<array>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime), C given
 - C<object>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime), C given
function Test<stdClass|DateTime,stdClass|DateTime>::m2(C<0|1>):
 - C<mixed>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTimeU = stdClass|DateTime), C given
 - C<int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTimeU = stdClass|DateTime), C given
 - C<string>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTimeU = stdClass|DateTime), C given
 - C<string|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTimeU = stdClass|DateTime), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTimeU = stdClass|DateTime), C given
 - C<stdClass|DateTime|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTimeU = stdClass|DateTime), C given
 - C<Base>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTimeU = stdClass|DateTime), C given
 - C<D>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTimeU = stdClass|DateTime), C given
 - C<C<int>>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTimeU = stdClass|DateTime), C given
 - C<C<stdClass>|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTimeU = stdClass|DateTime), C given
 - C<array>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTimeU = stdClass|DateTime), C given
 - C<object>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTimeU = stdClass|DateTime), C given
function Test<stdClass|DateTime,stdClass|DateTime>::m3(C<0|stdClass>):
 - C<mixed>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime), C given
 - C<int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime), C given
 - C<string>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime), C given
 - C<string|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime), C given
 - C<stdClass|DateTime|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime), C given
 - C<Base>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime), C given
 - C<D>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime), C given
 - C<C<int>>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime), C given
 - C<C<stdClass>|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime), C given
 - C<array>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime), C given
 - C<object>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime), C given
function Test<stdClass|DateTime,stdClass|DateTime>::m4(C<0|int>):
 - C<mixed>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime), C given
 - C<int>: accepted
 - C<string>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime), C given
 - C<string|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: accepted
 - C<stdClass|DateTime|int>: accepted
 - C<Base>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime), C given
 - C<D>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime), C given
 - C<C<int>>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime), C given
 - C<C<stdClass>|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime), C given
 - C<array>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime), C given
 - C<object>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime), C given
function Test<stdClass|DateTime,stdClass|DateTime>::m5(C<?0>):
 - C<mixed>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime), C given
 - C<int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime), C given
 - C<string>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime), C given
 - C<string|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime), C given
 - C<stdClass|DateTime|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime), C given
 - C<Base>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime), C given
 - C<D>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime), C given
 - C<C<int>>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime), C given
 - C<C<stdClass>|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime), C given
 - C<array>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime), C given
 - C<object>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime), C given
function Test<stdClass|DateTime,stdClass|DateTime>::m6(C<0&I>):
 - C<mixed>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime), C given
 - C<int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime), C given
 - C<string>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime), C given
 - C<string|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime), C given
 - C<stdClass>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime), C given
 - C<stdClass|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime), C given
 - C<stdClass|DateTime|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime), C given
 - C<Base>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime), C given
 - C<D>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime), C given
 - C<C<int>>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime), C given
 - C<C<stdClass>|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime), C given
 - C<array>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime), C given
 - C<object>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime), C given
function Test<stdClass|DateTime|int,stdClass|DateTime|int>::m1(C<0>):
 - C<mixed>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime|int), C given
 - C<int>: accepted
 - C<string>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime|int), C given
 - C<string|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime|int), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: accepted
 - C<stdClass|DateTime|int>: accepted
 - C<Base>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime|int), C given
 - C<D>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime|int), C given
 - C<C<int>>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime|int), C given
 - C<C<stdClass>|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime|int), C given
 - C<array>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime|int), C given
 - C<object>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = stdClass|DateTime|int), C given
function Test<stdClass|DateTime|int,stdClass|DateTime|int>::m2(C<0|1>):
 - C<mixed>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTime|intU = stdClass|DateTime|int), C given
 - C<int>: accepted
 - C<string>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTime|intU = stdClass|DateTime|int), C given
 - C<string|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTime|intU = stdClass|DateTime|int), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: accepted
 - C<stdClass|DateTime|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTime|intU = stdClass|DateTime|int), C given
 - C<Base>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTime|intU = stdClass|DateTime|int), C given
 - C<D>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTime|intU = stdClass|DateTime|int), C given
 - C<C<int>>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTime|intU = stdClass|DateTime|int), C given
 - C<C<stdClass>|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTime|intU = stdClass|DateTime|int), C given
 - C<array>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTime|intU = stdClass|DateTime|int), C given
 - C<object>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = stdClass|DateTime|intU = stdClass|DateTime|int), C given
function Test<stdClass|DateTime|int,stdClass|DateTime|int>::m3(C<0|stdClass>):
 - C<mixed>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime|int), C given
 - C<int>: accepted
 - C<string>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime|int), C given
 - C<string|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime|int), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: accepted
 - C<stdClass|DateTime|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime|int), C given
 - C<Base>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime|int), C given
 - C<D>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime|int), C given
 - C<C<int>>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime|int), C given
 - C<C<stdClass>|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime|int), C given
 - C<array>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime|int), C given
 - C<object>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = stdClass|DateTime|int), C given
function Test<stdClass|DateTime|int,stdClass|DateTime|int>::m4(C<0|int>):
 - C<mixed>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime|int), C given
 - C<int>: accepted
 - C<string>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime|int), C given
 - C<string|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime|int), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: accepted
 - C<stdClass|DateTime|int>: accepted
 - C<Base>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime|int), C given
 - C<D>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime|int), C given
 - C<C<int>>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime|int), C given
 - C<C<stdClass>|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime|int), C given
 - C<array>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime|int), C given
 - C<object>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = stdClass|DateTime|int), C given
function Test<stdClass|DateTime|int,stdClass|DateTime|int>::m5(C<?0>):
 - C<mixed>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime|int), C given
 - C<int>: accepted
 - C<string>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime|int), C given
 - C<string|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime|int), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: accepted
 - C<stdClass|DateTime|int>: accepted
 - C<Base>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime|int), C given
 - C<D>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime|int), C given
 - C<C<int>>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime|int), C given
 - C<C<stdClass>|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime|int), C given
 - C<array>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime|int), C given
 - C<object>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = stdClass|DateTime|int), C given
function Test<stdClass|DateTime|int,stdClass|DateTime|int>::m6(C<0&I>):
 - C<mixed>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime|int), C given
 - C<int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime|int), C given
 - C<string>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime|int), C given
 - C<string|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime|int), C given
 - C<stdClass>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime|int), C given
 - C<stdClass|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime|int), C given
 - C<stdClass|DateTime|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime|int), C given
 - C<Base>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime|int), C given
 - C<D>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime|int), C given
 - C<C<int>>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime|int), C given
 - C<C<stdClass>|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime|int), C given
 - C<array>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime|int), C given
 - C<object>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = stdClass|DateTime|int), C given
function Test<Base&I,Base&I>::m1(C<0>):
 - C<mixed>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base&I), C given
 - C<int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base&I), C given
 - C<string>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base&I), C given
 - C<string|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base&I), C given
 - C<stdClass>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base&I), C given
 - C<stdClass|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base&I), C given
 - C<stdClass|DateTime|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base&I), C given
 - C<Base>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base&I), C given
 - C<D>: accepted
 - C<C<int>>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base&I), C given
 - C<C<stdClass>|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base&I), C given
 - C<array>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base&I), C given
 - C<object>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base&I), C given
function Test<Base&I,Base&I>::m2(C<0|1>):
 - C<mixed>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base&IU = Base&I), C given
 - C<int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base&IU = Base&I), C given
 - C<string>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base&IU = Base&I), C given
 - C<string|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base&IU = Base&I), C given
 - C<stdClass>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base&IU = Base&I), C given
 - C<stdClass|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base&IU = Base&I), C given
 - C<stdClass|DateTime|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base&IU = Base&I), C given
 - C<Base>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base&IU = Base&I), C given
 - C<D>: accepted
 - C<C<int>>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base&IU = Base&I), C given
 - C<C<stdClass>|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base&IU = Base&I), C given
 - C<array>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base&IU = Base&I), C given
 - C<object>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base&IU = Base&I), C given
function Test<Base&I,Base&I>::m3(C<0|stdClass>):
 - C<mixed>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base&I), C given
 - C<int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base&I), C given
 - C<string>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base&I), C given
 - C<string|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base&I), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base&I), C given
 - C<stdClass|DateTime|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base&I), C given
 - C<Base>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base&I), C given
 - C<D>: accepted
 - C<C<int>>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base&I), C given
 - C<C<stdClass>|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base&I), C given
 - C<array>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base&I), C given
 - C<object>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base&I), C given
function Test<Base&I,Base&I>::m4(C<0|int>):
 - C<mixed>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base&I), C given
 - C<int>: accepted
 - C<string>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base&I), C given
 - C<string|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base&I), C given
 - C<stdClass>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base&I), C given
 - C<stdClass|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base&I), C given
 - C<stdClass|DateTime|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base&I), C given
 - C<Base>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base&I), C given
 - C<D>: accepted
 - C<C<int>>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base&I), C given
 - C<C<stdClass>|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base&I), C given
 - C<array>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base&I), C given
 - C<object>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base&I), C given
function Test<Base&I,Base&I>::m5(C<?0>):
 - C<mixed>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base&I), C given
 - C<int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base&I), C given
 - C<string>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base&I), C given
 - C<string|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base&I), C given
 - C<stdClass>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base&I), C given
 - C<stdClass|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base&I), C given
 - C<stdClass|DateTime|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base&I), C given
 - C<Base>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base&I), C given
 - C<D>: accepted
 - C<C<int>>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base&I), C given
 - C<C<stdClass>|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base&I), C given
 - C<array>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base&I), C given
 - C<object>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base&I), C given
function Test<Base&I,Base&I>::m6(C<0&I>):
 - C<mixed>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base&I), C given
 - C<int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base&I), C given
 - C<string>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base&I), C given
 - C<string|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base&I), C given
 - C<stdClass>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base&I), C given
 - C<stdClass|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base&I), C given
 - C<stdClass|DateTime|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base&I), C given
 - C<Base>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base&I), C given
 - C<D>: accepted
 - C<C<int>>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base&I), C given
 - C<C<stdClass>|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base&I), C given
 - C<array>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base&I), C given
 - C<object>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base&I), C given
function Test<Base|stdClass,int>::m1(C<0>):
 - C<mixed>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base|stdClass), C given
 - C<int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base|stdClass), C given
 - C<string>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base|stdClass), C given
 - C<string|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base|stdClass), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base|stdClass), C given
 - C<stdClass|DateTime|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base|stdClass), C given
 - C<Base>: accepted
 - C<D>: accepted
 - C<C<int>>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base|stdClass), C given
 - C<C<stdClass>|int>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base|stdClass), C given
 - C<array>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base|stdClass), C given
 - C<object>: Test::m1(): Argument #1 ($a) must be of type C<0> (where T = Base|stdClass), C given
function Test<Base|stdClass,int>::m2(C<0|1>):
 - C<mixed>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base|stdClassU = int), C given
 - C<int>: accepted
 - C<string>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base|stdClassU = int), C given
 - C<string|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base|stdClassU = int), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: accepted
 - C<stdClass|DateTime|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base|stdClassU = int), C given
 - C<Base>: accepted
 - C<D>: accepted
 - C<C<int>>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base|stdClassU = int), C given
 - C<C<stdClass>|int>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base|stdClassU = int), C given
 - C<array>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base|stdClassU = int), C given
 - C<object>: Test::m2(): Argument #1 ($a) must be of type C<0|1> (where T = Base|stdClassU = int), C given
function Test<Base|stdClass,int>::m3(C<0|stdClass>):
 - C<mixed>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base|stdClass), C given
 - C<int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base|stdClass), C given
 - C<string>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base|stdClass), C given
 - C<string|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base|stdClass), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base|stdClass), C given
 - C<stdClass|DateTime|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base|stdClass), C given
 - C<Base>: accepted
 - C<D>: accepted
 - C<C<int>>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base|stdClass), C given
 - C<C<stdClass>|int>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base|stdClass), C given
 - C<array>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base|stdClass), C given
 - C<object>: Test::m3(): Argument #1 ($a) must be of type C<0|stdClass> (where T = Base|stdClass), C given
function Test<Base|stdClass,int>::m4(C<0|int>):
 - C<mixed>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base|stdClass), C given
 - C<int>: accepted
 - C<string>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base|stdClass), C given
 - C<string|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base|stdClass), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: accepted
 - C<stdClass|DateTime|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base|stdClass), C given
 - C<Base>: accepted
 - C<D>: accepted
 - C<C<int>>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base|stdClass), C given
 - C<C<stdClass>|int>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base|stdClass), C given
 - C<array>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base|stdClass), C given
 - C<object>: Test::m4(): Argument #1 ($a) must be of type C<0|int> (where T = Base|stdClass), C given
function Test<Base|stdClass,int>::m5(C<?0>):
 - C<mixed>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base|stdClass), C given
 - C<int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base|stdClass), C given
 - C<string>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base|stdClass), C given
 - C<string|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base|stdClass), C given
 - C<stdClass>: accepted
 - C<stdClass|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base|stdClass), C given
 - C<stdClass|DateTime|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base|stdClass), C given
 - C<Base>: accepted
 - C<D>: accepted
 - C<C<int>>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base|stdClass), C given
 - C<C<stdClass>|int>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base|stdClass), C given
 - C<array>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base|stdClass), C given
 - C<object>: Test::m5(): Argument #1 ($a) must be of type C<?0> (where T = Base|stdClass), C given
function Test<Base|stdClass,int>::m6(C<0&I>):
 - C<mixed>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base|stdClass), C given
 - C<int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base|stdClass), C given
 - C<string>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base|stdClass), C given
 - C<string|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base|stdClass), C given
 - C<stdClass>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base|stdClass), C given
 - C<stdClass|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base|stdClass), C given
 - C<stdClass|DateTime|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base|stdClass), C given
 - C<Base>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base|stdClass), C given
 - C<D>: accepted
 - C<C<int>>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base|stdClass), C given
 - C<C<stdClass>|int>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base|stdClass), C given
 - C<array>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base|stdClass), C given
 - C<object>: Test::m6(): Argument #1 ($a) must be of type C<0&I> (where T = Base|stdClass), C given
