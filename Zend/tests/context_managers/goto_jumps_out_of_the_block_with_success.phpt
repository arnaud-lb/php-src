--TEST--
'goto' in using() jumps out of the block, considered a success
--FILE--
<?php

require 'basic_manager.inc';

using (new Manager() as $value) {
    echo "In using() block\n";
    goto out;
    echo "Not executed\n";
    var_dump($value);
}
out:

echo "After using() block\n";

?>
--EXPECT--
Manager::enterContext()
In using() block
Manager::exitContext(null)
After using() block
