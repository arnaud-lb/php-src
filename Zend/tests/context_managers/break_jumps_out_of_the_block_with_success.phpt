--TEST--
'break' in with() jumps out of the block, considered a success
--FILE--
<?php

require 'basic_manager.inc';

with (new Manager() as $value) {
    echo "In with() block\n";
    break;
    echo "Not executed\n";
    var_dump($value);
}

echo "After with() block\n";

?>
--EXPECT--
Manager::enterContext()
In with() block
Manager::exitContext(null)
After with() block
