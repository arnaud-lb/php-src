--TEST--
Variable on the rhs of 'as' is optional
--FILE--
<?php

require 'basic_manager.inc';

with (new Manager()) {
    echo "In with() block\n";
}

echo "After with() block\n";

?>
--EXPECT--
Manager::enterContext()
In with() block
Manager::exitContext(null)
After with() block
