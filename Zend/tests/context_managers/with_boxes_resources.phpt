--TEST--
with() boxes resources in ResourceContextManager
--FILE--
<?php

$fd = fopen("php://memory", "r");
with ($fd as $value) {
    echo "In with() block\n";
    var_dump($value);
}

echo "After with() block\n";
var_dump($fd);

?>
--EXPECTF--
In with() block
resource(%d) of type (stream)
After with() block
resource(%d) of type (Unknown)
