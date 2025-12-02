--TEST--
'using' keyword is semi reserved
--FILE--
<?php

class Test {
    const WITH = 1;
    public $using;
    function using ($using) {}
}

$using = 1;

// Not allowed:
// function using() {}
// class using {}
// const WITH = 1;

?>
==DONE==
--EXPECT--
==DONE==
