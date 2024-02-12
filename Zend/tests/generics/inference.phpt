--TEST--
Generic type inference
--FILE--
<?php

interface A {}
interface B {}
interface C {}

class GenA<T> {}
class GenB extends GenA<string> {}
class GenC<T> extends GenA<T> {}
class GenD<T,U> extends GenA<T|U> {}

class NoConstructor<T> {}

class TypeParamNotInConstructor<T> {
    public function __construct() {}
}

class RequiredTypeParam<T> {
    public function __construct(T $t) {}
}

class OptionalTypeParam<T,U=int> {
    public function __construct(T $t, U $u = null) {}
}

class ParamUsedTwice<T> {
    public function __construct(T $t, T $u) {}
}

class ParamInUnion<T> {
    public function __construct(T|A $t) {}
}

class ParamInNullable<T> {
    public function __construct(?T $t) {}
}

class ParamInGenericClass<T> {
    public function __construct(GenA<T> $t) {}
}

function test(A&B $ab, B&C $bc, A $a, B $b) {
    var_dump(new RequiredTypeParam("string"));
    var_dump(new RequiredTypeParam(1));
    var_dump(new RequiredTypeParam(new stdClass));

    var_dump(new OptionalTypeParam(1, "string"));
    var_dump(new OptionalTypeParam(1, 1));
    var_dump(new OptionalTypeParam(1, new stdClass));
    var_dump(new OptionalTypeParam(1));

    var_dump(new ParamUsedTwice(1, "string"));
    var_dump(new ParamUsedTwice(1, new stdClass));
    var_dump(new ParamUsedTwice(new DateTime, new stdClass));
    var_dump(new ParamUsedTwice($ab, new stdClass));
    var_dump(new ParamUsedTwice($ab, $ab));
    var_dump(new ParamUsedTwice($ab, $bc));
    var_dump(new ParamUsedTwice($ab, $a));
    var_dump(new ParamUsedTwice($a, $ab));

    var_dump(new ParamInUnion(new stdClass));
    var_dump(new ParamInUnion($a));
    var_dump(new ParamInUnion($ab));
    var_dump(new ParamInUnion(rand() ? $a : $b));

    var_dump(new ParamInNullable(new stdClass));
    var_dump(new ParamInNullable(rand() ? $a : null));
    var_dump(new ParamInNullable(rand() ? $b : null));

    var_dump(new ParamInGenericClass(new GenA<int>));
    var_dump(new ParamInGenericClass(new GenB));
    var_dump(new ParamInGenericClass(new GenC<int>));
    var_dump(new ParamInGenericClass(new GenD<int,string>));

    try {
        var_dump(new NoConstructor());
    } catch (Error $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }

    try {
        var_dump(new TypeParamNotInConstructor());
    } catch (Error $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }


}

test(
    new class implements A, B {},
    new class implements B, C {},
    new class implements A {},
    new class implements B {},
);

?>
--EXPECT--
object(RequiredTypeParam<string>)#5 (0) {
}
object(RequiredTypeParam<int>)#5 (0) {
}
object(RequiredTypeParam<stdClass>)#5 (0) {
}
object(OptionalTypeParam<int,string>)#5 (0) {
}
object(OptionalTypeParam<int,int>)#5 (0) {
}
object(OptionalTypeParam<int,stdClass>)#5 (0) {
}
object(OptionalTypeParam<int,int>)#5 (0) {
}
object(ParamUsedTwice<string|int>)#5 (0) {
}
object(ParamUsedTwice<stdClass|int>)#5 (0) {
}
object(ParamUsedTwice<DateTime|stdClass>)#5 (0) {
}
object(ParamUsedTwice<(A&B)|stdClass>)#5 (0) {
}
object(ParamUsedTwice<A&B>)#5 (0) {
}
object(ParamUsedTwice<(A&B)|(B&C)>)#5 (0) {
}
object(ParamUsedTwice<A>)#5 (0) {
}
object(ParamUsedTwice<A>)#5 (0) {
}
object(ParamInUnion<stdClass>)#5 (0) {
}
object(ParamInUnion<A>)#5 (0) {
}
object(ParamInUnion<A&B>)#5 (0) {
}
object(ParamInUnion<A|B>)#5 (0) {
}
object(ParamInNullable<stdClass>)#5 (0) {
}
object(ParamInNullable<?A>)#5 (0) {
}
object(ParamInNullable<?B>)#5 (0) {
}
object(ParamInGenericClass<int>)#5 (0) {
}
object(ParamInGenericClass<string>)#5 (0) {
}
object(ParamInGenericClass<int>)#5 (0) {
}
object(ParamInGenericClass<string|int>)#5 (0) {
}
Error: Generic type T can not be inferred because it is not referenced by a constructor parameter or no argument was passed to such parameter
Error: Generic type T can not be inferred because it is not referenced by a constructor parameter or no argument was passed to such parameter
