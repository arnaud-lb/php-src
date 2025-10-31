--TEST--
'with' keyword is semi reserved
--FILE--
<?php

class Test {
    const WITH = 1;
    public $with;
    function with ($with) {}
}

$with = 1;

// Not allowed:
// function with() {}
// class with {}
// const WITH = 1;

?>
==DONE==
--EXPECT--
==DONE==
