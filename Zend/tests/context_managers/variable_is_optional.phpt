--TEST--
Variable on the rhs of 'as' is optional
--FILE--
<?php

require 'basic_manager.inc';

using (new Manager()) {
    echo "In using() block\n";
}

echo "After using() block\n";

?>
--EXPECT--
Manager::enterContext()
In using() block
Manager::exitContext(null)
After using() block
