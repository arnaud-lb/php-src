<?php

namespace {

    const RootOnlyConstant = 'string';
    const RootAndNsConstant = 'string';

    class C<T> {
        public function __construct(T $t) {}
    }

    class D {
        const A = 1;
        const ?int B = 2;

        public $a;
        public int $b;
        public stdClass $c;
        public C<int> $d;
        public ?int $e;

        public function __construct() {
            $this->a = null;
            $this->b = 1;
            $this->c = new stdClass;
            $this->d = new C<int>(1);
            $this->e = null;
        }

        public static function staticMethod(): int {
            return 1;
        }

        public function method(): string {
            return "string";
        }

        public function d(): D {
            return $this;
        }
    }

    function _f($a): int {
        return (int) $a;
    }

    function _g($a): int {
        return (int) $a;
    }

    function _int(): int {
        return 1;
    }

    function _nullable_int(): ?int {
        return 1;
    }

    function _float(): float {
        return 1.5;
    }

    function _gmp(): GMP {
        return gmp_init(0);
    }

    function _array(): array {
        return [];
    }

    function _string(): string {
        return "string";
    }

    function _mixed(): mixed {
        return null;
    }

    function _mixed_long(): mixed {
        return 1;
    }

    function _extended_d(): D {
        return new class extends D {
            function extended(): D {
                return new D();
            }
        };
    }

}

namespace Ns {
    const RootAndNsConstant = 1;
}
