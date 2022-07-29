<?php

/** @generate-class-entries */

function dl_test_test1(): void {}

function dl_test_test2(string $str = ""): string {}

enum DlTestStringEnum: string {
    case Foo = "Test1";
    case Bar = "Test2";
    case Baz = "Test2\\a";
    case FortyTwo = "42";
}
