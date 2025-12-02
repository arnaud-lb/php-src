--TEST--
using() boxes resources in ResourceContextManager
--FILE--
<?php

$fd = fopen("php://memory", "r");
using ($fd as $value) {
    echo "In using() block\n";
    var_dump($value);
}

echo "After using() block\n";
var_dump($fd);

?>
--EXPECTF--
In using() block
resource(%d) of type (stream)
After using() block
resource(%d) of type (Unknown)
