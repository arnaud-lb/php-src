--TEST--
'continue' in with() jumps out of the block, considered a success, warns like 'switch'
--FILE--
<?php

require 'basic_manager.inc';

with (new Manager() as $value) {
    echo "In with() block\n";
    continue;
    echo "Not executed\n";
    var_dump($value);
}

echo "After with() block\n";

?>
--EXPECTF--
Warning: "continue" targeting with is equivalent to "break" in %s on line %d
Manager::enterContext()
In with() block
Manager::exitContext(null)
After with() block
