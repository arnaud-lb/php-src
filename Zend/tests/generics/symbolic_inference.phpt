--TEST--
Symbolic inference
--FILE--
<?php

namespace {

    require __DIR__ . '/symbolic_inference.inc';

    function recv() {
        $f = function($a, int $b, stdClass $c, C<int> $d, ?int $e = null) {
            var_dump(new C($a));
            var_dump(new C($b));
            var_dump(new C($c));
            var_dump(new C($d));
            var_dump(new C($e));
        };
        $f(1, 1, new stdClass, new C<int>(1), null);
    }

    function recv_variadic1() {
        $f = function(...$a) {
            var_dump(new C($a));
        };
        $f(1,2,3);
    }

    function recv_variadic2() {
        $f = function(int ...$a) {
            var_dump(new C($a));
        };
        $f(1,2,3);
    }

    function assign() {
        $a = 1;
        var_dump(new C($a));
        var_dump(new C($a = "string"));
    }

    function assign_dim($i=1) {
        $a[0] = 1;
        var_dump(new C($a[0]));
        var_dump(new C($a[1] = "string"));
        @var_dump(new C($a[2]|=1));
        @var_dump(new C($a[3].="string"));
    }

    function fetch_dim() {
        $a = [1];
        var_dump(new C($a[0]));
    }

    class J<T> {
        public T $t;
    }
    class KextendsJ<T> extends J<T> {
    }

    function fetch_obj() {
        $props = new D();
        var_dump(new C($props->a));
        var_dump(new C($props->b));
        var_dump(new C($props->c));
        var_dump(new C($props->d));
        var_dump(new C($props->e));

        $props = new J<int>();
        var_dump(new C($props->t ?? null));

        //$props = new KextendsJ<int>();
        //var_dump(new C($props->t ?? null));
    }

    class E {
        const DefinedInE = 'string';

        function useSelfDefinedInE() {
            var_dump(new C(self::DefinedInE));
        }

        function useStaticDefinedInE() {
            var_dump(new C(static::DefinedInE));
        }

        function useThisDefinedInE() {
            var_dump(new C($this::DefinedInE));
        }

        function useStaticDefinedInFFromE() {
            var_dump(new C(static::DefinedInF));
        }

        function useThisDefinedInFFromE() {
            var_dump(new C($this::DefinedInF));
        }

    }

    class FextendsE extends E {
        const DefinedInF = 1;

        function useSelfDefinedInEFromF() {
            var_dump(new C(self::DefinedInE));
        }

        function useParentDefinedInEFromF() {
            var_dump(new C(parent::DefinedInE));
        }

        function useStaticDefinedInEFromF() {
            var_dump(new C(parent::DefinedInE));
        }

        function useThisDefinedInEFromF() {
            var_dump(new C($this::DefinedInE));
        }

        function useSelfDefinedInF() {
            var_dump(new C(self::DefinedInF));
        }

        function useStaticDefinedInF() {
            var_dump(new C(static::DefinedInF));
        }

        function useThisDefinedInF() {
            var_dump(new C($this::DefinedInF));
        }
    }

    function fetch_class_constant() {
        var_dump(new C(D::A));
        var_dump(new C(D::B));

        $e = new E();
        foreach (get_class_methods($e) as $method) {
            printf("\$e->%s:\n", $method);
            try {
                [$e, $method]();
            } catch (Error $error) {
                printf("%s: %s\n", $error::class, $error->getMessage());
            }
        }

        $f = new FextendsE();
        foreach (get_class_methods($f) as $method) {
            printf("\$f->%s:\n", $method);
            try {
                [$f, $method]();
            } catch (Error $error) {
                printf("%s: %s\n", $error::class, $error->getMessage());
            }
        }
    }

    function fetch_constant1() {
        var_dump(new C(RootOnlyConstant));
        var_dump(new C(RootAndNsConstant));
    }

}

namespace Ns {

    function fetch_constant() {
        var_dump(new \C(RootOnlyConstant));
        var_dump(new \C(RootAndNsConstant));
    }

}

namespace {

    function bools(mixed $a = null, mixed $b = null) {
        var_dump(new C(!$a));
        var_dump(new C($a && $b));
        var_dump(new C($a || $b));
        var_dump(new C($a xor $b));
        var_dump(new C($a and $b));
        var_dump(new C($a or $b));
    }

    function arithmetic() {
        var_dump(new C(_int() + _int()));
        var_dump(new C(_array() + _array()));
        var_dump(new C(_mixed_long() + _mixed_long()));

        var_dump(new C(_int() * _int()));

        var_dump(new C(_int() ^ _int()));
        var_dump(new C(_string() ^ _string()));

        var_dump(new C(_int() % _int()));
    }

    function concat() {
        var_dump(new C(_string() . _string()));
        var_dump(new C(_int() . _int()));

        $a = _string();
        var_dump(new C("string $a"));
    }

    function fcall() {
        var_dump(new C(_int()));
    }

    class G {
        public static function f(): string {
            return 'string';
        }
        public function callSelfF() {
            var_dump(new C(self::f()));
        }
        public function callStaticF() {
            var_dump(new C(static::f()));
        }
        public function callThisF() {
            var_dump(new C($this::f()));
        }
    }

    class H<T> {
        public function __construct(public mixed $p) {}
        public function f(): T {
            return $this->p;
        }
        public function callSelfF() {
            var_dump(new C(self::f()));
        }
        public function callStaticF() {
            var_dump(new C(static::f()));
        }
        public function callThisF() {
            var_dump(new C($this::f()));
        }
    }

    class IextendsH<T> extends H<T> {
        public function callParentF() {
            var_dump(new C(parent::f()));
        }
    }

    function mcall() {
        $o = new D();
        var_dump(new C($o->staticMethod()));
        var_dump(new C(D::staticMethod()));
        var_dump(new C($o->method()));

        $o = _extended_d();
        var_dump(new C($o->extended()));

        $g = new G();
        printf("\$g->callSelfF\n");
        $g->callSelfF();
        printf("\$g->callStaticF\n");
        $g->callStaticF();
        printf("\$g->callThisF\n");
        $g->callThisF();

        $h = new H<string>('string');
        printf("\$h->callSelfF\n");
        $h->callSelfF();
        printf("\$h->callStaticF\n");
        $h->callStaticF();
        printf("\$h->callThisF\n");
        $h->callThisF();

        printf("\$h->f\n");
        var_dump(new C($h->f()));

        $i = new IextendsH<string>('string');
        printf("\$i->callParentF\n");
        $i->callParentF();
    }

    function new_() {
        var_dump(new C(new D()));
        var_dump(new C(new C<int>(1)));
        var_dump(new C(new C(1)));
    }

    function coalesce() {
        var_dump(new C(_nullable_int() ?? _string()));
        var_dump(new C(_nullable_int() ?? null));
    }

    function isset_() {
        var_dump(new C(isset($a)));
        var_dump(new C(empty($a)));
    }

    function lambda() {
        var_dump(new C(function () {}));
    }

    function assign_obj() {
        $d = new D();
        $c = new C<int>(1);
        var_dump(new C($d->a = 1));
        var_dump(new C($d->b = 1));
        var_dump(new C($d->d = &$c));
        var_dump(new C($d->b += 1));
    }

    function init_array(): void {
        $a = [rand(), rand()];
        var_dump(new C($a));
        $a = [...$a];
        var_dump(new C($a));
    }

    function phi(): void {
        $a = 1;
        if (rand()) {
            $a = 'string';
        }
        var_dump(new C($a));
    }

    function recursive_fcall($i = 1): void {
        $a = "string";
        do {
            // _f(...(_f(_f(string|_f(string)))) -> _f(string|_f(string))
            $a = _f($a);
        } while ($i--);
        var_dump(new C($a));
    }

    function recursive_fcall2($i = 1): void {
        $a = "string";
        do {
            $a = _f($a);
            $a = _g($a);
        } while ($i--);
        var_dump(new C($a));
    }

    function recursive_arith1($i = 1): void {
        $a = "0";
        do {
            $a = $a + 1;
        } while ($i--);
        var_dump(new C($a));
    }

    function recursive_arith2($i = 1): void {
        $a = "0";
        do {
            $a = _f($a + 1);
        } while ($i--);
        var_dump(new C($a));
    }

    function recursive_mcall($i = 1): void {
        $d = new D();
        do {
            $d = $d->d();
        } while ($i--);
        var_dump(new C($d));
    }

    // TODO:
    // - @
    // - assign_op

    foreach (get_defined_functions()['user'] as $function) {
        if (str_starts_with($function, '_')) {
            continue;
        }

        printf("\n# %s\n", $function);

        $function();
    }

}

?>
--EXPECTF--
# %s
object(C<mixed>)#4 (0) {
}
object(C<int>)#4 (0) {
}
object(C<stdClass>)#4 (0) {
}
object(C<C<int>>)#4 (0) {
}
object(C<?int>)#4 (0) {
}

# recv_variadic1
object(C<array>)#3 (0) {
}

# recv_variadic2
object(C<array>)#3 (0) {
}

# assign
object(C<int>)#1 (0) {
}
object(C<string>)#1 (0) {
}

# assign_dim
object(C<mixed>)#1 (0) {
}
object(C<string>)#1 (0) {
}
object(C<int>)#1 (0) {
}
object(C<string>)#1 (0) {
}

# fetch_dim
object(C<mixed>)#1 (0) {
}

# fetch_obj
object(C<mixed>)#4 (0) {
}
object(C<int>)#4 (0) {
}
object(C<stdClass>)#4 (0) {
}
object(C<C<int>>)#4 (0) {
}
object(C<?int>)#4 (0) {
}
object(C<?int>)#1 (0) {
}

# fetch_class_constant
object(C<int>)#4 (0) {
}
object(C<?int>)#4 (0) {
}
$e->useSelfDefinedInE:
object(C<string>)#1 (0) {
}
$e->useStaticDefinedInE:
object(C<mixed>)#1 (0) {
}
$e->useThisDefinedInE:
object(C<string>)#1 (0) {
}
$e->useStaticDefinedInFFromE:
Error: Undefined constant E::DefinedInF
$e->useThisDefinedInFFromE:
Error: Undefined constant E::DefinedInF
$f->useSelfDefinedInEFromF:
object(C<string>)#1 (0) {
}
$f->useParentDefinedInEFromF:
object(C<string>)#1 (0) {
}
$f->useStaticDefinedInEFromF:
object(C<string>)#1 (0) {
}
$f->useThisDefinedInEFromF:
object(C<string>)#1 (0) {
}
$f->useSelfDefinedInF:
object(C<int>)#1 (0) {
}
$f->useStaticDefinedInF:
object(C<mixed>)#1 (0) {
}
$f->useThisDefinedInF:
object(C<int>)#1 (0) {
}
$f->useSelfDefinedInE:
object(C<string>)#1 (0) {
}
$f->useStaticDefinedInE:
object(C<mixed>)#1 (0) {
}
$f->useThisDefinedInE:
object(C<string>)#1 (0) {
}
$f->useStaticDefinedInFFromE:
object(C<mixed>)#1 (0) {
}
$f->useThisDefinedInFFromE:
object(C<mixed>)#1 (0) {
}

# fetch_constant1
object(C<string>)#2 (0) {
}
object(C<string>)#2 (0) {
}

# ns\fetch_constant
object(C<string>)#2 (0) {
}
object(C<int>)#2 (0) {
}

# bools
object(C<bool>)#2 (0) {
}
object(C<bool>)#2 (0) {
}
object(C<bool>)#2 (0) {
}
object(C<bool>)#2 (0) {
}
object(C<bool>)#2 (0) {
}
object(C<bool>)#2 (0) {
}

# arithmetic
object(C<int|float>)#2 (0) {
}
object(C<array>)#2 (0) {
}
object(C<int|float>)#2 (0) {
}
object(C<int|float>)#2 (0) {
}
object(C<int>)#2 (0) {
}
object(C<string>)#2 (0) {
}
object(C<int>)#2 (0) {
}

# concat
object(C<string>)#2 (0) {
}
object(C<string>)#2 (0) {
}
object(C<string>)#2 (0) {
}

# fcall
object(C<int>)#2 (0) {
}

# mcall
object(C<int>)#1 (0) {
}
object(C<mixed>)#1 (0) {
}
object(C<string>)#1 (0) {
}
object(C<mixed>)#2 (0) {
}
$g->callSelfF
object(C<string>)#4 (0) {
}
$g->callStaticF
object(C<mixed>)#4 (0) {
}
$g->callThisF
object(C<string>)#4 (0) {
}
$h->callSelfF
object(C<string>)#7 (0) {
}
$h->callStaticF
object(C<mixed>)#7 (0) {
}
$h->callThisF
object(C<string>)#7 (0) {
}
$h->f
object(C<string>)#7 (0) {
}
$i->callParentF
object(C<string>)#3 (0) {
}

# new_
object(C<D>)#7 (0) {
}
object(C<C<int>>)#7 (0) {
}
object(C<C>)#7 (0) {
}

# coalesce
object(C<string|int>)#7 (0) {
}
object(C<?int>)#7 (0) {
}

# isset_
object(C<bool>)#7 (0) {
}
object(C<bool>)#7 (0) {
}

# lambda
object(C<Closure>)#7 (0) {
}

# assign_obj
object(C<int>)#6 (0) {
}
object(C<int>)#6 (0) {
}
object(C<C<int>>)#6 (0) {
}
object(C<int|float>)#6 (0) {
}

# init_array
object(C<array>)#2 (0) {
}
object(C<array>)#2 (0) {
}

# phi
object(C<string|int>)#2 (0) {
}

# recursive_fcall
object(C<int>)#2 (0) {
}

# recursive_fcall2
object(C<int>)#2 (0) {
}

# recursive_arith1
object(C<int|float>)#2 (0) {
}

# recursive_arith2
object(C<int>)#2 (0) {
}

# recursive_mcall
object(C<D>)#6 (0) {
}
