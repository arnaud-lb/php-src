--TEST--
'continue' in using() jumps out of the block, considered a success, warns like 'switch'
--FILE--
<?php

require 'basic_manager.inc';

using (new Manager() as $value) {
    echo "In using() block\n";
    continue;
    echo "Not executed\n";
    var_dump($value);
}

echo "After using() block\n";

?>
--EXPECTF--
Warning: "continue" targeting using is equivalent to "break" in %s on line %d
Manager::enterContext()
In using() block
Manager::exitContext(null)
After using() block
