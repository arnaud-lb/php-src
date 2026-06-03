--TEST--
Stream hook: closed during read hook
--XFAIL--
UAF in _php_stream_get_line
--FILE--
<?php

stream_set_hook(function ($stream) {
    fclose($stream);
    stream_set_hook(null);
});

$fd = fopen(__FILE__, 'r');
var_dump(fgets($fd));
?>
--EXPECTF--
